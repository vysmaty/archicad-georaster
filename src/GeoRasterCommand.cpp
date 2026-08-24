#include "GeoRasterPrecompiledHeader.hpp"

#include "GeoRasterCommand.hpp"
#include "ResourceIds.hpp"
#include "RS.hpp"
#include "compat/ArchicadCompatibility.hpp"
#include "core/GeoRasterCore.hpp"
#include "core/WorksheetWorkflow.hpp"
#include "ui/ImportDialog.hpp"

#include <filesystem>
#include <optional>

namespace GeoRasterCommand {
namespace {

GS::UniString Text(short index)
{
    return RSGetIndString(ID_STATUS_STRINGS, index, ACCompat::OwnResourceModule());
}

void ShowError(const GS::UniString& message)
{
    DGAlert(DG_ERROR, Text(29), message, GS::UniString(), Text(30));
}

GS::UniString NextWorksheetReference()
{
    for (int index = 1; index <= 9999; ++index) {
        const GS::UniString reference = GS::UniString::Printf("GR-%03d", index);
        if (!ACCompat::WorksheetReferenceExists(reference)) {
            return reference;
        }
    }
    return {};
}

struct PreparedImport {
    GeoRaster::RasterInfo raster;
    GeoRaster::WorldFootprint footprint;
    std::vector<std::byte> bytes;
};

std::optional<PreparedImport> Prepare(const GeoRasterUI::ImportRequest& request)
{
    const auto raster = GeoRaster::ReadRasterInfo(request.rasterPath);
    const auto worldFile = GeoRaster::ReadWorldFile(request.worldFilePath);
    const auto bytes = GeoRaster::ReadBinaryFile(request.rasterPath);
    if (!raster.IsValid() || !worldFile.IsValid() || !bytes.IsValid()) {
        ShowError(Text(31));
        return std::nullopt;
    }
    return PreparedImport {
        *raster.value,
        GeoRaster::ComputeWorldFootprint(
            *worldFile.value, raster.value->pixelWidth, raster.value->pixelHeight
        ),
        std::move(*bytes.value)
    };
}

GSErrCode ImportToWorksheet(
    const GeoRasterUI::ImportRequest& request,
    const PreparedImport& prepared,
    API_DatabaseInfo& originalDatabase,
    const API_Element& pictureDefaults,
    GS::UniString& errorStage
)
{
    const auto placement = GeoRaster::ComputeWorksheetPlacement(prepared.footprint);
    const GS::UniString reference = NextWorksheetReference();
    if (reference.IsEmpty()) {
        ShowError(Text(32));
        return APIERR_GENERAL;
    }
    const GS::UniString name = Text(36) + GS::ToUniString(request.rasterPath.stem().wstring());

    const auto workflow = GeoRaster::RunWorksheetWorkflow<API_DatabaseInfo, GSErrCode>(
        originalDatabase,
        NoError,
        [&](API_DatabaseInfo& worksheet) {
            errorStage = "CreateWorksheet";
            return ACCompat::CreateWorksheet(reference, name, worksheet);
        },
        [&](API_DatabaseInfo& database) {
            errorStage = "ChangeCurrentDatabase";
            return ACCompat::ChangeCurrentDatabase(database);
        },
        [&]() {
            errorStage = "CallUndoable";
            return ACCompat::CallUndoable(Text(34), [&]() {
                errorStage = "CreatePicture";
                return ACCompat::CreatePicture(
                    pictureDefaults,
                    prepared.raster,
                    prepared.bytes,
                    placement.localBounds.minimum,
                    placement.localBounds.maximum.x,
                    placement.localBounds.maximum.y,
                    0.0
                );
            });
        },
        [&](API_DatabaseInfo& worksheet) {
            return ACCompat::DeleteWorksheet(worksheet);
        }
    );
    if (workflow.restore != NoError || workflow.cleanup != NoError) {
        ACCompat::ReportRollbackFailure(workflow.restore, workflow.cleanup);
    }
    return workflow.primary;
}

GSErrCode ImportToFloorPlan(
    const PreparedImport& prepared,
    const GeoRaster::Affine2D& projectToSurvey,
    const API_Element& pictureDefaults
)
{
    const auto placement = GeoRaster::ComputeFloorPlanPlacement(prepared.footprint, projectToSurvey);
    if (!placement.IsValid()) {
        ShowError(Text(35));
        return APIERR_BADPARS;
    }
    return ACCompat::CallUndoable(Text(34), [&]() {
        return ACCompat::CreatePicture(
            pictureDefaults,
            prepared.raster,
            prepared.bytes,
            placement.value->anchor,
            placement.value->width,
            placement.value->height,
            placement.value->rotation
        );
    });
}

} // namespace

void Run()
{
    API_DatabaseInfo originalDatabase {};
    GSErrCode error = ACCompat::GetCurrentDatabase(originalDatabase);
    if (error != NoError) {
        ShowError(Text(37) + " " + ACCompat::ErrorText(error));
        return;
    }

    API_Element pictureDefaults {};
    error = ACCompat::GetPictureDefaults(pictureDefaults);
    if (error != NoError) {
        ShowError(Text(37) + " " + ACCompat::ErrorText(error));
        return;
    }

    const bool invokedFromFloorPlan = originalDatabase.typeID == APIWind_FloorPlanID;
    std::optional<GeoRaster::Affine2D> projectToSurvey;
    if (invokedFromFloorPlan) {
        GeoRaster::Affine2D transform;
        if (ACCompat::GetProjectToSurveyTransform(transform) == NoError) {
            projectToSurvey = transform;
        }
    }

    GeoRasterUI::ImportDialog dialog(invokedFromFloorPlan && projectToSurvey.has_value(), projectToSurvey);
    if (!dialog.Invoke()) {
        return;
    }
    const GeoRasterUI::ImportRequest request = dialog.GetRequest();
    const auto prepared = Prepare(request);
    if (!prepared) {
        return;
    }

    GS::UniString errorStage;
    if (request.target == GeoRasterUI::ImportTarget::NewWorksheet) {
        error = ImportToWorksheet(
            request, *prepared, originalDatabase, pictureDefaults, errorStage
        );
    } else {
        if (!invokedFromFloorPlan || !projectToSurvey) {
            ShowError(Text(9));
            return;
        }
        error = ImportToFloorPlan(*prepared, *projectToSurvey, pictureDefaults);
    }
    if (error != NoError) {
        GS::UniString message = Text(37) + " " + ACCompat::ErrorText(error);
        if (!errorStage.IsEmpty()) {
            message += "\n" + Text(38) + " " + errorStage;
        }
        ShowError(message);
    }
}

} // namespace GeoRasterCommand
