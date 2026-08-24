#pragma once

#include "ACAPinc.h"
#include "core/GeoRasterCore.hpp"

#include <functional>
#include <vector>

namespace ACCompat {

struct WorksheetHandle {
    API_DatabaseInfo database;
    API_WindowInfo window;
};

struct WorksheetChoice {
    GS::UniString label;
    API_WindowInfo window;
};

GSErrCode RegisterMenu(short menuResourceId);
GSResModule OwnResourceModule();
void ReportRollbackFailure(GSErrCode restoreError, GSErrCode deleteError);
GSErrCode GetCurrentWindow(API_WindowInfo& window);
GSErrCode ActivateWindow(const API_WindowInfo& window);
GSErrCode CreateWorksheet(
    const GS::UniString& reference,
    const GS::UniString& name,
    WorksheetHandle& worksheet
);
GSErrCode DeleteWorksheet(WorksheetHandle& worksheet);
bool WorksheetReferenceExists(const GS::UniString& reference);
std::vector<WorksheetChoice> GetWorksheetChoices();
GSErrCode GetProjectToSurveyTransform(GeoRaster::Affine2D& transform);
GSErrCode GetProjectLengthUnit(GeoRaster::LengthUnitInfo& unit);
GSErrCode FormatProjectLength(double meters, GS::UniString& formatted);
GSErrCode CreatePicture(
    const GeoRaster::RasterInfo& raster,
    const std::vector<std::byte>& bytes,
    GeoRaster::Point2D anchor,
    double width,
    double height,
    double rotation
);
GSErrCode CreateStaticDrawing(
    const GeoRaster::RasterInfo& raster,
    const std::vector<std::byte>& bytes,
    GeoRaster::Point2D anchor,
    double width,
    double height,
    double rotation
);
GSErrCode CallUndoable(const GS::UniString& label, const std::function<GSErrCode()>& command);
GS::UniString ErrorText(GSErrCode error);

template<typename MenuHandler>
GSErrCode InstallMenuHandler(short menuResourceId, MenuHandler handler)
{
    return ACAPI_MenuItem_InstallMenuHandler(menuResourceId, handler);
}

} // namespace ACCompat
