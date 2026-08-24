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

GSErrCode GetCurrentWindow(API_WindowInfo& window)
{
    window = {};
    return ACAPI_Window_GetCurrentWindow(&window);
}

GSErrCode ActivateWindow(const API_WindowInfo& window)
{
    return ACAPI_Window_ChangeWindow(&window);
}

GSErrCode CreateWorksheet(
    const GS::UniString& reference,
    const GS::UniString& name,
    WorksheetHandle& worksheet
)
{
    worksheet = {};
    worksheet.database.typeID = APIWind_WorksheetID;
    GS::ucscpy(worksheet.database.ref, reference.ToUStr().Get());
    GS::ucscpy(worksheet.database.name, name.ToUStr().Get());
    const GSErrCode error = ACAPI_Database_NewDatabase(&worksheet.database);
    if (error == NoError) {
        worksheet.window.typeID = APIWind_WorksheetID;
        worksheet.window.databaseUnId = worksheet.database.databaseUnId;
    }
    return error;
}

GSErrCode DeleteWorksheet(WorksheetHandle& worksheet)
{
    return ACAPI_Database_DeleteDatabase(&worksheet.database);
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

std::vector<WorksheetChoice> GetWorksheetChoices()
{
    std::vector<WorksheetChoice> choices;
    const auto databases = ACAPI_Database_GetDatabasesForType(APIWind_WorksheetID);
    for (const API_DatabaseUnId& databaseId : databases) {
        API_DatabaseInfo database {};
        database.typeID = APIWind_WorksheetID;
        database.databaseUnId = databaseId;
        if (ACAPI_Window_GetDatabaseInfo(&database) != NoError) {
            continue;
        }
        WorksheetChoice choice;
        choice.label = GS::UniString(database.ref) + " — " + GS::UniString(database.name);
        choice.window.typeID = APIWind_WorksheetID;
        choice.window.databaseUnId = database.databaseUnId;
        choices.push_back(std::move(choice));
    }
    return choices;
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

GSErrCode GetProjectLengthUnit(GeoRaster::LengthUnitInfo& unit)
{
    API_WorkingUnitPrefs preferences {};
    const GSErrCode error = ACAPI_ProjectSetting_GetPreferences(
        &preferences, APIPrefs_WorkingUnitsID
    );
    if (error != NoError) {
        unit = {};
        return error;
    }

    unit.available = true;
    switch (preferences.lengthUnit) {
        case API_LengthTypeID::Meter: unit = {1.0, "m", true}; break;
        case API_LengthTypeID::Decimeter: unit = {0.1, "dm", true}; break;
        case API_LengthTypeID::Centimeter: unit = {0.01, "cm", true}; break;
        case API_LengthTypeID::Millimeter: unit = {0.001, "mm", true}; break;
        case API_LengthTypeID::FootFracInch: unit = {0.3048, "ft/in", true}; break;
        case API_LengthTypeID::FootDecInch: unit = {0.3048, "ft/in", true}; break;
        case API_LengthTypeID::DecFoot: unit = {0.3048, "ft", true}; break;
        case API_LengthTypeID::FracInch: unit = {0.0254, "in", true}; break;
        case API_LengthTypeID::DecInch: unit = {0.0254, "in", true}; break;
        case API_LengthTypeID::KiloMeter: unit = {1000.0, "km", true}; break;
        case API_LengthTypeID::Yard: unit = {0.9144, "yd", true}; break;
        default:
            unit = {};
            return APIERR_BADID;
    }
    return NoError;
}

GSErrCode FormatProjectLength(double meters, GS::UniString& formatted)
{
    API_UnitConversionData conversion {};
    conversion.value = meters;
    conversion.unitConvPref = APIUnitConv_WorkModel;
    const GSErrCode error = ACAPI_Conversion_GetConvertedUnitValue(&conversion);
    if (error != NoError) {
        formatted.Clear();
        return error;
    }
    formatted = GS::UniString(conversion.convertedValue);
    if (conversion.unit[0] != 0) {
        formatted += " ";
        formatted += GS::UniString(conversion.unit);
    }
    return NoError;
}

GSErrCode CreatePicture(
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

    API_Element element {};
    element.header.type = API_PictureID;
    GSErrCode error = ACAPI_Element_GetDefaults(&element, nullptr);
    if (error != NoError) {
        return error;
    }

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

GSErrCode CreateStaticDrawing(
    const GeoRaster::RasterInfo& raster,
    const std::vector<std::byte>& bytes,
    GeoRaster::Point2D anchor,
    double width,
    double height,
    double rotation
)
{
    if (bytes.empty() || width <= 0.0 || height <= 0.0) {
        return APIERR_BADPARS;
    }

    GSErrCode error = ACAPI_Drawing_StartDrawingData();
    if (error != NoError) {
        return error;
    }

    error = CreatePicture(raster, bytes, {0.0, 0.0}, width, height, 0.0);
    GSPtr drawingData = nullptr;
    API_Box bounds {};
    const GSErrCode stopError = ACAPI_Drawing_StopDrawingData(&drawingData, &bounds);
    if (error != NoError) {
        if (drawingData != nullptr) {
            BMKillPtr(&drawingData);
        }
        return error;
    }
    if (stopError != NoError || drawingData == nullptr) {
        if (drawingData != nullptr) {
            BMKillPtr(&drawingData);
        }
        return stopError != NoError ? stopError : APIERR_GENERAL;
    }

    API_Element drawing {};
    drawing.header.type = API_DrawingID;
    error = ACAPI_Element_GetDefaults(&drawing, nullptr);
    if (error == NoError) {
        drawing.drawing.pos = {anchor.x, anchor.y};
        drawing.drawing.angle = rotation;
        drawing.drawing.ratio = 1.0;
        drawing.drawing.anchorPoint = APIAnc_LB;
        drawing.drawing.useOwnOrigoAsAnchor = true;
        drawing.drawing.isTransparentBk = true;
        drawing.drawing.isCutWithFrame = false;
        drawing.drawing.hasBorderLine = false;
        drawing.drawing.manualUpdate = true;
        drawing.drawing.nameType = APIName_CustomName;
        CHCopyC("GeoRaster", drawing.drawing.name);
        drawing.drawing.bounds = bounds;
        API_ElementMemo memo {};
        memo.drawingData = drawingData;
        error = ACAPI_Element_Create(&drawing, &memo);
    }
    BMKillPtr(&drawingData);
    return error;
}

GSErrCode CallUndoable(const GS::UniString& label, const std::function<GSErrCode()>& command)
{
    return ACAPI_CallUndoableCommand(label, command);
}

GS::UniString ErrorText(GSErrCode error)
{
    if (error == APIERR_BADINDEX) {
        return GS::UniString::Printf("APIERR_BADINDEX (%d)", error);
    }
    if (error == APIERR_BADDATABASE) {
        return GS::UniString::Printf("APIERR_BADDATABASE (%d)", error);
    }
    return GS::UniString::Printf("%d", error);
}

} // namespace ACCompat
