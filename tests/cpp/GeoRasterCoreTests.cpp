#include "core/GeoRasterCore.hpp"
#include "core/WorksheetWorkflow.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

bool Near(double actual, double expected, double tolerance = 1.0e-9)
{
    return std::abs(actual - expected) <= tolerance;
}

void TestWorldFileParser()
{
    const auto parsed = GeoRaster::ParseWorldFile("0.25\n0\n0\n-0.25\n1.2e6\n-3.4E5\n");
    Expect(parsed.IsValid(), "standard world file parses");
    Expect(parsed.value && Near(parsed.value->a, 0.25), "A is parsed");
    Expect(parsed.value && Near(parsed.value->f, -340000.0), "F exponent is parsed");

    Expect(
        GeoRaster::ParseWorldFile("1,5\n0\n0\n-1\n0\n0").validation.code ==
            GeoRaster::ValidationCode::InvalidWorldFileNumber,
        "decimal comma is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("1\n0\n0\n-1\n0").validation.code ==
            GeoRaster::ValidationCode::InvalidWorldFileValueCount,
        "five values are rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("1\n0\n0\n-1\n0\n0\n7").validation.code ==
            GeoRaster::ValidationCode::InvalidWorldFileValueCount,
        "seven values are rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("nan\n0\n0\n-1\n0\n0").validation.code !=
            GeoRaster::ValidationCode::Ok,
        "NaN is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("inf\n0\n0\n-1\n0\n0").validation.code !=
            GeoRaster::ValidationCode::Ok,
        "infinity is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("0\n0\n0\n-1\n0\n0").validation.code ==
            GeoRaster::ValidationCode::ZeroPixelScale,
        "zero scale is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("-1\n0\n0\n-1\n0\n0").validation.code ==
            GeoRaster::ValidationCode::MirroredRaster,
        "mirroring is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("1\n0.1\n0\n-1\n0\n0").validation.code ==
            GeoRaster::ValidationCode::RotatedOrShearedRaster,
        "rotation is rejected"
    );
    Expect(
        GeoRaster::ParseWorldFile("1\n0\n0.1\n-1\n0\n0").validation.code ==
            GeoRaster::ValidationCode::RotatedOrShearedRaster,
        "shear is rejected"
    );
}

void TestFootprintAndWorksheet()
{
    const GeoRaster::WorldFileTransform transform {2.0, 0.0, 0.0, -3.0, 100.0, 200.0};
    const auto footprint = GeoRaster::ComputeWorldFootprint(transform, 4, 2);
    Expect(Near(footprint.topLeft.x, 99.0) && Near(footprint.topLeft.y, 201.5),
           "top-left outer corner applies half-pixel correction");
    Expect(Near(footprint.bottomRight.x, 107.0) && Near(footprint.bottomRight.y, 195.5),
           "bottom-right outer corner applies half-pixel correction");

    const auto worksheet = GeoRaster::ComputeWorksheetPlacement(footprint);
    Expect(Near(worksheet.worldOrigin.x, 99.0) && Near(worksheet.worldOrigin.y, 195.5),
           "worksheet origin is south-west outer corner");
    Expect(Near(worksheet.localBounds.maximum.x, 8.0) &&
               Near(worksheet.localBounds.maximum.y, 6.0),
           "worksheet bounds use full raster size");
}

void TestRasterHeaders()
{
    const auto folder = std::filesystem::temp_directory_path() / "georaster-core-tests";
    std::filesystem::create_directories(folder);
    const auto pngPath = folder / "sample.png";
    const std::vector<unsigned char> png {
        137, 80, 78, 71, 13, 10, 26, 10,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 1, 44, 0, 0, 0, 200
    };
    {
        std::ofstream output(pngPath, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    }
    const auto pngInfo = GeoRaster::ReadRasterInfo(pngPath);
    Expect(pngInfo.IsValid() && pngInfo.value->pixelWidth == 300 &&
               pngInfo.value->pixelHeight == 200,
           "PNG dimensions are read from IHDR");

    const auto jpegPath = folder / "sample.jpg";
    const std::vector<unsigned char> jpeg {
        0xFF, 0xD8,
        0xFF, 0xE0, 0x00, 0x04, 0x00, 0x00,
        0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x01, 0x90, 0x02, 0x80, 0x01, 0x01, 0x11, 0x00,
        0xFF, 0xD9
    };
    {
        std::ofstream output(jpegPath, std::ios::binary);
        output.write(reinterpret_cast<const char*>(jpeg.data()), static_cast<std::streamsize>(jpeg.size()));
    }
    const auto jpegInfo = GeoRaster::ReadRasterInfo(jpegPath);
    Expect(jpegInfo.IsValid() && jpegInfo.value->pixelWidth == 640 &&
               jpegInfo.value->pixelHeight == 400,
           "JPEG dimensions are read from SOF");

    const auto corruptPngPath = folder / "corrupt.png";
    {
        std::ofstream output(corruptPngPath, std::ios::binary);
        output << "not a PNG";
    }
    Expect(
        GeoRaster::ReadRasterInfo(corruptPngPath).validation.code ==
            GeoRaster::ValidationCode::CorruptPng,
        "damaged PNG is rejected"
    );
    const auto corruptJpegPath = folder / "corrupt.jpg";
    {
        std::ofstream output(corruptJpegPath, std::ios::binary);
        output << "not a JPEG";
    }
    Expect(
        GeoRaster::ReadRasterInfo(corruptJpegPath).validation.code ==
            GeoRaster::ValidationCode::CorruptJpeg,
        "damaged JPEG is rejected"
    );

    {
        std::ofstream output(folder / "sample.pgw");
        output << "1\n0\n0\n-1\n0\n0\n";
    }
    {
        std::ofstream output(folder / "sample.pngw");
        output << "1\n0\n0\n-1\n0\n0\n";
    }
    const auto discovered = GeoRaster::DiscoverWorldFiles(pngPath);
    Expect(discovered.candidates.size() == 2, "all world-file candidates are returned");
    Expect(discovered.preferred && discovered.preferred->extension() == ".pgw",
           "world-file precedence prefers PGW");
}

void TestSurveyPlacement()
{
    const GeoRaster::WorldFootprint footprint {
        {100.0, 220.0}, {140.0, 220.0}, {140.0, 200.0}, {100.0, 200.0}
    };
    auto identity = GeoRaster::ComputeFloorPlanPlacement(footprint, {});
    Expect(identity.IsValid(), "identity survey transformation is valid");
    Expect(identity.value && Near(identity.value->anchor.x, 100.0) &&
               Near(identity.value->anchor.y, 200.0),
           "identity keeps anchor");

    GeoRaster::Affine2D translation;
    translation.tx = 100.0;
    translation.ty = 200.0;
    auto translated = GeoRaster::ComputeFloorPlanPlacement(footprint, translation);
    Expect(translated.IsValid() && Near(translated.value->anchor.x, 0.0) &&
               Near(translated.value->anchor.y, 0.0),
           "project-to-survey translation is inverted");

    GeoRaster::Affine2D rotation;
    rotation.m00 = 0.0;
    rotation.m01 = -1.0;
    rotation.m10 = 1.0;
    rotation.m11 = 0.0;
    auto rotated = GeoRaster::ComputeFloorPlanPlacement(footprint, rotation);
    Expect(rotated.IsValid() && Near(rotated.value->rotation, -1.5707963267948966),
           "survey rotation is inverted into picture rotation");

    GeoRaster::Affine2D combined = rotation;
    combined.tx = 100.0;
    combined.ty = 200.0;
    Expect(GeoRaster::ComputeFloorPlanPlacement(footprint, combined).IsValid(),
           "combined translation and rotation is accepted");

    GeoRaster::Affine2D singular;
    singular.m00 = 0.0;
    singular.m11 = 0.0;
    Expect(
        GeoRaster::ComputeFloorPlanPlacement(footprint, singular).validation.code ==
            GeoRaster::ValidationCode::SingularSurveyTransform,
        "singular survey transform is rejected"
    );

    GeoRaster::Affine2D scale;
    scale.m00 = 2.0;
    Expect(
        GeoRaster::ComputeFloorPlanPlacement(footprint, scale).validation.code ==
            GeoRaster::ValidationCode::NonRigidSurveyTransform,
        "non-rigid survey transform is rejected"
    );

    const GeoRaster::WorldFootprint farFootprint {
        {10001.0, 1.0}, {10002.0, 1.0}, {10002.0, 0.0}, {10001.0, 0.0}
    };
    Expect(
        GeoRaster::ComputeFloorPlanPlacement(farFootprint, {}).validation.code ==
            GeoRaster::ValidationCode::TooFarFromProjectOrigin,
        "10 km safety limit is enforced"
    );
}

void TestWorksheetRollback()
{
    std::vector<std::string> events;
    const int original = 7;
    const auto result = GeoRaster::RunWorksheetWorkflow<int, int>(
        original,
        0,
        [&](int& created) {
            created = 9;
            events.emplace_back("create");
            return 0;
        },
        [&](int& database) {
            events.emplace_back(database == 9 ? "switch-new" : "restore-original");
            return 0;
        },
        [&]() {
            events.emplace_back("insert-picture");
            return 42;
        },
        [&](int& created) {
            Expect(created == 9, "rollback deletes the newly created worksheet");
            events.emplace_back("delete-new");
            return 0;
        }
    );
    Expect(result.primary == 42 && result.restore == 0 && result.cleanup == 0,
           "picture error is preserved after successful rollback");
    const std::vector<std::string> expected {
        "create", "switch-new", "insert-picture", "restore-original", "delete-new"
    };
    Expect(events == expected, "rollback restores original database before deleting worksheet");
}

} // namespace

int main()
{
    TestWorldFileParser();
    TestFootprintAndWorksheet();
    TestRasterHeaders();
    TestSurveyPlacement();
    TestWorksheetRollback();
    if (failures == 0) {
        std::cout << "All GeoRaster core tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
