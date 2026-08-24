#pragma once

#include "ACAPinc.h"
#include "core/GeoRasterCore.hpp"

#include <functional>
#include <vector>

namespace ACCompat {

GSErrCode RegisterMenu(short menuResourceId);
GSResModule OwnResourceModule();
void ReportRollbackFailure(GSErrCode restoreError, GSErrCode deleteError);
GSErrCode GetCurrentDatabase(API_DatabaseInfo& database);
GSErrCode ChangeCurrentDatabase(API_DatabaseInfo& database);
GSErrCode GetPictureDefaults(API_Element& element);
GSErrCode CreateWorksheet(
    const GS::UniString& reference,
    const GS::UniString& name,
    API_DatabaseInfo& database
);
GSErrCode DeleteWorksheet(API_DatabaseInfo& database);
bool WorksheetReferenceExists(const GS::UniString& reference);
GSErrCode GetProjectToSurveyTransform(GeoRaster::Affine2D& transform);
GSErrCode CreatePicture(
    const API_Element& defaults,
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
