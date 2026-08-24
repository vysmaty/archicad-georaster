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

GSErrCode CreateOutput(
    const PreparedImport& prepared,
    GeoRaster::Point2D anchor,
    double width,
    double height,
    double rotation,
    GS::UniString& errorStage
)
{
    return ACCompat::CallUndoable(Text(34), [&]() {
        errorStage = "CreatePicture";
        return ACCompat::CreatePicture(
            prepared.raster, prepared.bytes, anchor, width, height, rotation
        );
    });
}

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
    const API_WindowInfo& originalWindow,
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

    const auto workflow = GeoRaster::RunWorksheetWorkflow<
        API_WindowInfo, ACCompat::WorksheetHandle, GSErrCode
    >(
        originalWindow,
        NoError,
        [&](ACCompat::WorksheetHandle& worksheet) {
            errorStage = "CreateWorksheet";
            return ACCompat::CreateWorksheet(reference, name, worksheet);
        },
        [&](ACCompat::WorksheetHandle& worksheet) {
            errorStage = "ActivateWorksheet";
            return ACCompat::ActivateWindow(worksheet.window);
        },
        [&]() {
            errorStage = "CallUndoable";
            return CreateOutput(
                prepared, placement.anchor, placement.width, placement.height, 0.0, errorStage
            );
        },
        [&](const API_WindowInfo& window) {
            return ACCompat::ActivateWindow(window);
        },
        [&](ACCompat::WorksheetHandle& worksheet) {
            return ACCompat::DeleteWorksheet(worksheet);
        }
    );
    if (workflow.restore != NoError || workflow.cleanup != NoError) {
        ACCompat::ReportRollbackFailure(workflow.restore, workflow.cleanup);
    }
    return workflow.primary;
}

GSErrCode ImportToExistingWorksheet(
    const PreparedImport& prepared,
    const API_WindowInfo& originalWindow,
    const API_WindowInfo& targetWindow,
    GS::UniString& errorStage
)
{
    errorStage = "ActivateWorksheet";
    GSErrCode error = ACCompat::ActivateWindow(targetWindow);
    if (error != NoError) {
        return error;
    }
    const auto placement = GeoRaster::ComputeWorksheetPlacement(prepared.footprint);
    error = CreateOutput(
        prepared, placement.anchor, placement.width, placement.height, 0.0, errorStage
    );
    if (error != NoError) {
        const GSErrCode restoreError = ACCompat::ActivateWindow(originalWindow);
        if (restoreError != NoError) {
            ACCompat::ReportRollbackFailure(restoreError, NoError);
        }
    }
    return error;
}

GSErrCode ImportToFloorPlan(
    const PreparedImport& prepared,
    const GeoRaster::Affine2D& projectToSurvey,
    GS::UniString& errorStage
)
{
    const auto placement = GeoRaster::ComputeFloorPlanPlacement(prepared.footprint, projectToSurvey);
    if (!placement.IsValid()) {
        ShowError(Text(35));
        return APIERR_BADPARS;
    }
    return CreateOutput(
        prepared, placement.value->anchor, placement.value->width,
        placement.value->height, placement.value->rotation, errorStage
    );
}

} // namespace

void Run()
{
    API_WindowInfo originalWindow {};
    GSErrCode error = ACCompat::GetCurrentWindow(originalWindow);
    if (error != NoError) {
        ShowError(Text(37) + " " + ACCompat::ErrorText(error));
        return;
    }

    const bool invokedFromFloorPlan = originalWindow.typeID == APIWind_FloorPlanID;
    const bool invokedFromWorksheet = originalWindow.typeID == APIWind_WorksheetID;
    std::optional<GeoRaster::Affine2D> projectToSurvey;
    if (invokedFromFloorPlan) {
        GeoRaster::Affine2D transform;
        if (ACCompat::GetProjectToSurveyTransform(transform) == NoError) {
            projectToSurvey = transform;
        }
    }

    std::optional<GeoRaster::LengthUnitInfo> projectLengthUnit;
    GeoRaster::LengthUnitInfo unit;
    if (ACCompat::GetProjectLengthUnit(unit) == NoError) {
        projectLengthUnit = unit;
    }

    GeoRasterUI::ImportDialog dialog(
        invokedFromFloorPlan && projectToSurvey.has_value(), invokedFromWorksheet,
        ACCompat::GetWorksheetChoices(), projectToSurvey, projectLengthUnit
    );
    if (!dialog.Invoke()) {
        return;
    }
    const GeoRasterUI::ImportRequest request = dialog.GetRequest();
    const auto prepared = Prepare(request);
    if (!prepared) {
        return;
    }

    GS::UniString errorStage;
    switch (request.target) {
        case GeoRasterUI::ImportTarget::NewWorksheet:
            error = ImportToWorksheet(request, *prepared, originalWindow, errorStage);
            break;
        case GeoRasterUI::ImportTarget::ActiveWorksheet:
            if (!invokedFromWorksheet) {
                ShowError(Text(54));
                return;
            }
            error = ImportToExistingWorksheet(
                *prepared, originalWindow, originalWindow, errorStage
            );
            break;
        case GeoRasterUI::ImportTarget::SelectedWorksheet:
            if (!request.worksheetWindow) {
                ShowError(Text(54));
                return;
            }
            error = ImportToExistingWorksheet(
                *prepared, originalWindow, *request.worksheetWindow, errorStage
            );
            break;
        case GeoRasterUI::ImportTarget::ActiveFloorPlan:
            if (!invokedFromFloorPlan || !projectToSurvey) {
                ShowError(Text(9));
                return;
            }
            error = ImportToFloorPlan(*prepared, *projectToSurvey, errorStage);
            break;
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
