#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GeoRaster {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Bounds2D {
    Point2D minimum;
    Point2D maximum;
};

struct WorldFileTransform {
    double a = 0.0;
    double d = 0.0;
    double b = 0.0;
    double e = 0.0;
    double c = 0.0;
    double f = 0.0;
};

enum class RasterFormat { PNG, JPEG };

struct RasterInfo {
    std::filesystem::path path;
    RasterFormat format = RasterFormat::PNG;
    std::uint32_t pixelWidth = 0;
    std::uint32_t pixelHeight = 0;
};

struct WorldFootprint {
    Point2D topLeft;
    Point2D topRight;
    Point2D bottomRight;
    Point2D bottomLeft;
};

struct WorksheetPlacement {
    Point2D worldOrigin;
    Bounds2D localBounds;
};

struct Affine2D {
    double m00 = 1.0;
    double m01 = 0.0;
    double m10 = 0.0;
    double m11 = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

struct LengthUnitInfo {
    double metersPerUnit = 1.0;
    std::string symbol = "m";
    bool available = false;
};

struct FloorPlanPlacement {
    WorldFootprint localCorners;
    Point2D anchor;
    double width = 0.0;
    double height = 0.0;
    double rotation = 0.0;
};

struct SurveyPlacementDiagnostics {
    Point2D surveyPointProjectPosition;
    Point2D projectOriginSurveyCoordinates;
    double maximumCornerDistance = 0.0;
    std::optional<double> suggestedSurveyPointScale;
};

enum class ValidationCode {
    Ok,
    FileNotFound,
    FileReadFailed,
    InvalidWorldFileNumber,
    InvalidWorldFileValueCount,
    NonFiniteWorldFileValue,
    ZeroPixelScale,
    MirroredRaster,
    RotatedOrShearedRaster,
    UnsupportedRasterFormat,
    CorruptPng,
    CorruptJpeg,
    MissingWorldFile,
    MultipleWorldFiles,
    NonFiniteSurveyTransform,
    NonRigidSurveyTransform,
    SingularSurveyTransform,
    ProbableSurveyPointUnitMismatch,
    TooFarFromProjectOrigin
};

struct ValidationResult {
    ValidationCode code = ValidationCode::Ok;
    std::vector<std::string> arguments;

    [[nodiscard]] bool IsValid() const { return code == ValidationCode::Ok; }
    static ValidationResult Success() { return {}; }
    static ValidationResult Failure(ValidationCode code, std::vector<std::string> arguments = {});
};

template<typename T>
struct Validated {
    std::optional<T> value;
    ValidationResult validation;

    [[nodiscard]] bool IsValid() const { return value.has_value() && validation.IsValid(); }
};

struct FloorPlanPlacementAnalysis {
    std::optional<FloorPlanPlacement> placement;
    SurveyPlacementDiagnostics diagnostics;
    ValidationResult validation;

    [[nodiscard]] bool IsValid() const
    {
        return placement.has_value() && validation.IsValid();
    }
};

struct WorldFileDiscovery {
    std::vector<std::filesystem::path> candidates;
    std::optional<std::filesystem::path> preferred;
};

Validated<WorldFileTransform> ParseWorldFile(std::string_view text);
Validated<WorldFileTransform> ReadWorldFile(const std::filesystem::path& path);
WorldFileDiscovery DiscoverWorldFiles(const std::filesystem::path& rasterPath);
Validated<RasterInfo> ReadRasterInfo(const std::filesystem::path& path);
Validated<std::vector<std::byte>> ReadBinaryFile(const std::filesystem::path& path);

WorldFootprint ComputeWorldFootprint(
    const WorldFileTransform& transform,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight
);
WorksheetPlacement ComputeWorksheetPlacement(const WorldFootprint& footprint);
Validated<FloorPlanPlacement> ComputeFloorPlanPlacement(
    const WorldFootprint& surveyFootprint,
    const Affine2D& projectToSurvey,
    double maximumDistance = 10000.0
);
FloorPlanPlacementAnalysis AnalyzeFloorPlanPlacement(
    const WorldFootprint& surveyFootprint,
    const Affine2D& projectToSurvey,
    const LengthUnitInfo& projectLengthUnit = {},
    double maximumDistance = 10000.0
);
Bounds2D ComputeBounds(const WorldFootprint& footprint);

} // namespace GeoRaster
