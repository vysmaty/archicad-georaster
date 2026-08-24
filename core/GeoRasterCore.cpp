#include "core/GeoRasterCore.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace GeoRaster {
namespace {

Point2D TransformPixel(const WorldFileTransform& transform, double column, double row)
{
    return {
        transform.a * column + transform.b * row + transform.c,
        transform.d * column + transform.e * row + transform.f
    };
}

bool IsFinite(const Affine2D& transform)
{
    return std::isfinite(transform.m00) && std::isfinite(transform.m01) &&
           std::isfinite(transform.m10) && std::isfinite(transform.m11) &&
           std::isfinite(transform.tx) && std::isfinite(transform.ty);
}

double Distance(Point2D first, Point2D second = {})
{
    return std::hypot(first.x - second.x, first.y - second.y);
}

} // namespace

ValidationResult ValidationResult::Failure(ValidationCode code, std::vector<std::string> arguments)
{
    return {code, std::move(arguments)};
}

Validated<WorldFileTransform> ParseWorldFile(std::string_view text)
{
    if (text.find(',') != std::string_view::npos) {
        return {{}, ValidationResult::Failure(ValidationCode::InvalidWorldFileNumber)};
    }

    std::istringstream stream(std::string{text});
    stream.imbue(std::locale::classic());
    std::array<std::string, 6> tokens;
    for (std::string& token : tokens) {
        if (!(stream >> token)) {
            return {{}, ValidationResult::Failure(ValidationCode::InvalidWorldFileValueCount)};
        }
    }
    std::string trailing;
    if (stream >> trailing) {
        return {{}, ValidationResult::Failure(ValidationCode::InvalidWorldFileValueCount)};
    }

    std::array<double, 6> values {};
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string& token = tokens[index];
        const auto parsed = std::from_chars(
            token.data(), token.data() + token.size(), values[index], std::chars_format::general
        );
        if (parsed.ec != std::errc {} || parsed.ptr != token.data() + token.size()) {
            return {{}, ValidationResult::Failure(ValidationCode::InvalidWorldFileNumber, {token})};
        }
        if (!std::isfinite(values[index])) {
            return {{}, ValidationResult::Failure(ValidationCode::NonFiniteWorldFileValue)};
        }
    }

    WorldFileTransform transform {
        values[0], values[1], values[2], values[3], values[4], values[5]
    };
    const double tolerance = 1.0e-12 * std::max({std::abs(transform.a), std::abs(transform.e), 1.0});
    if (std::abs(transform.a) <= tolerance || std::abs(transform.e) <= tolerance) {
        return {{}, ValidationResult::Failure(ValidationCode::ZeroPixelScale)};
    }
    if (transform.a < 0.0 || transform.e > 0.0) {
        return {{}, ValidationResult::Failure(ValidationCode::MirroredRaster)};
    }
    if (std::abs(transform.b) > tolerance || std::abs(transform.d) > tolerance) {
        return {{}, ValidationResult::Failure(ValidationCode::RotatedOrShearedRaster)};
    }
    transform.b = 0.0;
    transform.d = 0.0;
    return {transform, ValidationResult::Success()};
}

Validated<WorldFileTransform> ReadWorldFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {{}, ValidationResult::Failure(
            std::filesystem::exists(path) ? ValidationCode::FileReadFailed : ValidationCode::FileNotFound,
            {path.u8string()}
        )};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return ParseWorldFile(buffer.str());
}

WorldFootprint ComputeWorldFootprint(
    const WorldFileTransform& transform,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight
)
{
    const double right = static_cast<double>(pixelWidth) - 0.5;
    const double bottom = static_cast<double>(pixelHeight) - 0.5;
    return {
        TransformPixel(transform, -0.5, -0.5),
        TransformPixel(transform, right, -0.5),
        TransformPixel(transform, right, bottom),
        TransformPixel(transform, -0.5, bottom)
    };
}

Bounds2D ComputeBounds(const WorldFootprint& footprint)
{
    const std::array<Point2D, 4> corners {
        footprint.topLeft, footprint.topRight, footprint.bottomRight, footprint.bottomLeft
    };
    Bounds2D bounds {corners[0], corners[0]};
    for (const Point2D corner : corners) {
        bounds.minimum.x = std::min(bounds.minimum.x, corner.x);
        bounds.minimum.y = std::min(bounds.minimum.y, corner.y);
        bounds.maximum.x = std::max(bounds.maximum.x, corner.x);
        bounds.maximum.y = std::max(bounds.maximum.y, corner.y);
    }
    return bounds;
}

WorksheetPlacement ComputeWorksheetPlacement(const WorldFootprint& footprint)
{
    const Bounds2D worldBounds = ComputeBounds(footprint);
    return {
        worldBounds.minimum,
        worldBounds.maximum.x - worldBounds.minimum.x,
        worldBounds.maximum.y - worldBounds.minimum.y
    };
}

FloorPlanPlacementAnalysis AnalyzeFloorPlanPlacement(
    const WorldFootprint& surveyFootprint,
    const Affine2D& projectToSurvey,
    const LengthUnitInfo& projectLengthUnit,
    double maximumDistance
)
{
    if (!IsFinite(projectToSurvey)) {
        return {{}, {}, ValidationResult::Failure(ValidationCode::NonFiniteSurveyTransform)};
    }

    const double determinant = projectToSurvey.m00 * projectToSurvey.m11 -
                               projectToSurvey.m01 * projectToSurvey.m10;
    if (std::abs(determinant) <= 1.0e-12) {
        return {{}, {}, ValidationResult::Failure(ValidationCode::SingularSurveyTransform)};
    }

    constexpr double rigidTolerance = 1.0e-9;
    const double column0Length = std::hypot(projectToSurvey.m00, projectToSurvey.m10);
    const double column1Length = std::hypot(projectToSurvey.m01, projectToSurvey.m11);
    const double dot = projectToSurvey.m00 * projectToSurvey.m01 +
                       projectToSurvey.m10 * projectToSurvey.m11;
    if (determinant < 0.0 || std::abs(column0Length - 1.0) > rigidTolerance ||
        std::abs(column1Length - 1.0) > rigidTolerance || std::abs(dot) > rigidTolerance) {
        return {{}, {}, ValidationResult::Failure(ValidationCode::NonRigidSurveyTransform)};
    }

    const auto inverse = [&](Point2D survey) {
        const double x = survey.x - projectToSurvey.tx;
        const double y = survey.y - projectToSurvey.ty;
        return Point2D {
            (projectToSurvey.m11 * x - projectToSurvey.m01 * y) / determinant,
            (-projectToSurvey.m10 * x + projectToSurvey.m00 * y) / determinant
        };
    };

    FloorPlanPlacement placement;
    placement.localCorners = {
        inverse(surveyFootprint.topLeft), inverse(surveyFootprint.topRight),
        inverse(surveyFootprint.bottomRight), inverse(surveyFootprint.bottomLeft)
    };
    const std::array<Point2D, 4> localCorners {
        placement.localCorners.topLeft, placement.localCorners.topRight,
        placement.localCorners.bottomRight, placement.localCorners.bottomLeft
    };
    SurveyPlacementDiagnostics diagnostics;
    diagnostics.surveyPointProjectPosition = inverse({});
    diagnostics.projectOriginSurveyCoordinates = {projectToSurvey.tx, projectToSurvey.ty};
    for (const Point2D corner : localCorners) {
        diagnostics.maximumCornerDistance = std::max(
            diagnostics.maximumCornerDistance, Distance(corner)
        );
    }

    placement.anchor = placement.localCorners.bottomLeft;
    placement.width = Distance(placement.localCorners.bottomLeft, placement.localCorners.bottomRight);
    placement.height = Distance(placement.localCorners.bottomLeft, placement.localCorners.topLeft);
    placement.rotation = std::atan2(
        placement.localCorners.bottomRight.y - placement.localCorners.bottomLeft.y,
        placement.localCorners.bottomRight.x - placement.localCorners.bottomLeft.x
    );
    if (diagnostics.maximumCornerDistance > maximumDistance) {
        std::ostringstream distance;
        distance.imbue(std::locale::classic());
        distance << std::fixed << std::setprecision(3) << diagnostics.maximumCornerDistance;

        const bool usableUnit = projectLengthUnit.available &&
            std::isfinite(projectLengthUnit.metersPerUnit) &&
            projectLengthUnit.metersPerUnit > 0.0;
        if (usableUnit) {
            const double suggestedScale = 1.0 / projectLengthUnit.metersPerUnit;
            if (std::abs(suggestedScale - 1.0) > 1.0e-12) {
                Affine2D correctedTransform = projectToSurvey;
                correctedTransform.tx *= suggestedScale;
                correctedTransform.ty *= suggestedScale;
                const auto corrected = AnalyzeFloorPlanPlacement(
                    surveyFootprint, correctedTransform, {}, maximumDistance
                );
                if (corrected.IsValid()) {
                    diagnostics.suggestedSurveyPointScale = suggestedScale;
                    return {
                        {}, diagnostics,
                        ValidationResult::Failure(
                            ValidationCode::ProbableSurveyPointUnitMismatch,
                            {distance.str()}
                        )
                    };
                }
            }
        }
        return {
            {}, diagnostics,
            ValidationResult::Failure(
                ValidationCode::TooFarFromProjectOrigin, {distance.str()}
            )
        };
    }
    return {placement, diagnostics, ValidationResult::Success()};
}

Validated<FloorPlanPlacement> ComputeFloorPlanPlacement(
    const WorldFootprint& surveyFootprint,
    const Affine2D& projectToSurvey,
    double maximumDistance
)
{
    const auto analysis = AnalyzeFloorPlanPlacement(
        surveyFootprint, projectToSurvey, {}, maximumDistance
    );
    return {analysis.placement, analysis.validation};
}

} // namespace GeoRaster
