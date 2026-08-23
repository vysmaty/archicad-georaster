#include "GeoRasterPrecompiledHeader.hpp"

#include "GeoRasterCommand.hpp"
#include "ResourceIds.hpp"
#include "RS.hpp"
#include "compat/ArchicadCompatibility.hpp"

namespace {

constexpr GSResID AddOnInfoID = ID_ADDON_INFO;
constexpr Int32 AddOnNameID = 1;
constexpr Int32 AddOnDescriptionID = 2;
constexpr short AddOnMenuID = ID_ADDON_MENU;

GSErrCode MenuCommandHandler(const API_MenuParams* menuParams)
{
    if (menuParams->menuItemRef.menuResID == AddOnMenuID) {
        GeoRasterCommand::Run();
    }
    return NoError;
}

} // namespace

API_AddonType CheckEnvironment(API_EnvirParams* envir)
{
    RSGetIndString(&envir->addOnInfo.name, AddOnInfoID, AddOnNameID, ACAPI_GetOwnResModule());
    RSGetIndString(&envir->addOnInfo.description, AddOnInfoID, AddOnDescriptionID, ACAPI_GetOwnResModule());
    return APIAddon_Normal;
}

GSErrCode RegisterInterface()
{
    return ACCompat::RegisterMenu(AddOnMenuID);
}

GSErrCode Initialize()
{
    return ACCompat::InstallMenuHandler(AddOnMenuID, MenuCommandHandler);
}

GSErrCode FreeData()
{
    return NoError;
}
