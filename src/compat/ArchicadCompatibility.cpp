#include "GeoRasterPrecompiledHeader.hpp"

#include "compat/ArchicadCompatibility.hpp"

#include <cstring>
#include <cmath>

namespace ACCompat {

GSErrCode RegisterMenu(short menuResourceId)
{
    return ACAPI_MenuItem_RegisterMenu(
        menuResourceId, 0, MenuCode_Tools, MenuFlag_Default
    );
}

GSResModule OwnResourceModule()
{
    return ACAPI_GetOwnResModule();
}

void ReportRollbackFailure(GSErrCode restoreError, GSErrCode deleteError)
{
    ACAPI_WriteReport("GeoRaster rollback failed: restore=%d delete=%d", true,
                      restoreError, deleteError);
}

GSErrCode GetCurrentDatabase(API_DatabaseInfo& database)
{
    database = {};
    return ACAPI_Database_GetCurrentDatabase(&database);
}

GSErrCode ChangeCurrentDatabase(API_DatabaseInfo& database)
{
    return ACAPI_Database_ChangeCurrentDatabase(&database);
}

GSErrCode GetPictureDefaults(API_Element& element)
{
    element = {};
    element.header.type = API_PictureID;
    return ACAPI_Element_GetDefaults(&element, nullptr);
}

GSErrCode CreateWorksheet(
    const GS::UniString& reference,
    const GS::UniString& name,
    API_DatabaseInfo& database
)
{
    database = {};
    database.typeID = APIWind_WorksheetID;
    GS::ucscpy(database.ref, reference.ToUStr().Get());
    GS::ucscpy(database.name, name.ToUStr().Get());
    return ACAPI_Database_NewDatabase(&database);
}

GSErrCode DeleteWorksheet(API_DatabaseInfo& database)
{
    return ACAPI_Database_DeleteDatabase(&database);
}

bool WorksheetReferenceExists(const GS::UniString& reference)
{
    const auto databases = ACAPI_Database_GetDatabasesForType(APIWind_WorksheetID);
    for (const API_DatabaseUnId& databaseId : databases) {
        API_DatabaseInfo database {};
        database.typeID = APIWind_WorksheetID;
        database.databaseUnId = databaseId;
        if (ACAPI_Window_GetDatabaseInfo(&database) == NoError &&
            GS::UniString(database.ref) == reference) {
            return true;
        }
    }
    return false;
}

GSErrCode GetProjectToSurveyTransform(GeoRaster::Affine2D& transform)
{
    API_Tranmat matrix {};
    const GSErrCode error = ACAPI_SurveyPoint_GetSurveyPointTransformation(&matrix);
    if (error != NoError) {
        return error;
    }
    for (double value : matrix.tmx) {
        if (!std::isfinite(value)) {
            return APIERR_BADPARS;
        }
    }
    constexpr double tolerance = 1.0e-9;
    if (std::abs(matrix.tmx[2]) > tolerance || std::abs(matrix.tmx[6]) > tolerance ||
        std::abs(matrix.tmx[8]) > tolerance || std::abs(matrix.tmx[9]) > tolerance ||
        std::abs(matrix.tmx[10] - 1.0) > tolerance) {
        return APIERR_BADPARS;
    }
    transform = {
        matrix.tmx[0], matrix.tmx[1], matrix.tmx[4], matrix.tmx[5],
        matrix.tmx[3], matrix.tmx[7]
    };
    return NoError;
}

GSErrCode CreatePicture(
    const API_Element& defaults,
    const GeoRaster::RasterInfo& raster,
    const std::vector<std::byte>& bytes,
    GeoRaster::Point2D anchor,
    double width,
    double height,
    double rotation
)
{
    if (bytes.empty()) {
        return APIERR_BADPARS;
    }

    API_Element element = defaults;
    GSErrCode error = NoError;

    element.picture.usePixelSize = false;
    element.picture.mirrored = false;
    element.picture.transparent = false;
    element.picture.directCreate = false;
    element.picture.destBox.xMin = anchor.x;
    element.picture.destBox.yMin = anchor.y;
    element.picture.destBox.xMax = anchor.x + width;
    element.picture.destBox.yMax = anchor.y + height;
    element.picture.rotAngle = rotation;
    element.picture.anchorPoint = APIAnc_LB;
    element.picture.storageFormat = raster.format == GeoRaster::RasterFormat::PNG
        ? APIPictForm_PNG
        : APIPictForm_JPEG;
    GS::ucscpy(element.picture.pictName, L("GeoRaster"));

    API_ElementMemo memo {};
    memo.pictHdl = BMAllocateHandle(static_cast<GSSize>(bytes.size()), ALLOCATE_CLEAR, 0);
    if (memo.pictHdl == nullptr) {
        return APIERR_MEMFULL;
    }
    std::memcpy(*memo.pictHdl, bytes.data(), bytes.size());
    error = ACAPI_Element_Create(&element, &memo);
    ACAPI_DisposeElemMemoHdls(&memo);
    return error;
}

GSErrCode CallUndoable(const GS::UniString& label, const std::function<GSErrCode()>& command)
{
    return ACAPI_CallUndoableCommand(label, command);
}

GS::UniString ErrorText(GSErrCode error)
{
    return GS::UniString::Printf("%d", error);
}

} // namespace ACCompat
