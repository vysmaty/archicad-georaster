#include "GeoRasterPrecompiledHeader.hpp"

#include "ResourceIds.hpp"
#include "RS.hpp"
#include "compat/ArchicadCompatibility.hpp"
#include "ui/ImportDialog.hpp"

#include <iomanip>
#include <locale>
#include <sstream>

namespace GeoRasterUI {
namespace {

GS::UniString ResourceText(short index)
{
    return RSGetIndString(ID_STATUS_STRINGS, index, ACCompat::OwnResourceModule());
}

GS::UniString Number(double value, int precision = 3)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(precision) << value;
    return GS::UniString(stream.str().c_str(), CC_UTF8);
}

} // namespace

ImportDialog::ImportDialog(
    bool floorPlanAvailable,
    std::optional<GeoRaster::Affine2D> projectToSurvey,
    std::optional<GeoRaster::LengthUnitInfo> projectLengthUnit
)
    : DG::ModalDialog(ACCompat::OwnResourceModule(), ID_IMPORT_DIALOG, InvalidResModule),
      rasterEdit(GetReference(), RasterEditId),
      rasterBrowse(GetReference(), RasterBrowseId),
      worldFileEdit(GetReference(), WorldFileEditId),
      worldFileBrowse(GetReference(), WorldFileBrowseId),
      worksheetRadio(GetReference(), WorksheetRadioId),
      floorPlanRadio(GetReference(), FloorPlanRadioId),
      previewText(GetReference(), PreviewTextId),
      importButton(GetReference(), ImportButtonId),
      cancelButton(GetReference(), CancelButtonId),
      floorPlanAvailable(floorPlanAvailable),
      projectToSurvey(std::move(projectToSurvey)),
      projectLengthUnit(std::move(projectLengthUnit))
{
    rasterBrowse.Attach(*this);
    worldFileBrowse.Attach(*this);
    importButton.Attach(*this);
    cancelButton.Attach(*this);
    worksheetRadio.Attach(*this);
    floorPlanRadio.Attach(*this);
    rasterEdit.Attach(*this);
    worldFileEdit.Attach(*this);

    worksheetRadio.Select();
    if (!floorPlanAvailable) {
        floorPlanRadio.Disable();
    }
    importButton.Disable();
    previewText.SetText(ResourceText(1));
}

ImportDialog::~ImportDialog()
{
    rasterBrowse.Detach(*this);
    worldFileBrowse.Detach(*this);
    importButton.Detach(*this);
    cancelButton.Detach(*this);
    worksheetRadio.Detach(*this);
    floorPlanRadio.Detach(*this);
    rasterEdit.Detach(*this);
    worldFileEdit.Detach(*this);
}

ImportRequest ImportDialog::GetRequest() const
{
    return {
        GetPath(rasterEdit),
        GetPath(worldFileEdit),
        floorPlanRadio.IsSelected() ? ImportTarget::ActiveFloorPlan : ImportTarget::NewWorksheet
    };
}

void ImportDialog::ButtonClicked(const DG::ButtonClickEvent& event)
{
    if (event.GetSource() == &rasterBrowse) {
        std::filesystem::path path;
        if (SelectFile({"png", "jpg", "jpeg"}, ResourceText(2), path)) {
            SetPath(rasterEdit, path);
            AutoSelectWorldFile();
            RefreshValidation();
        }
    } else if (event.GetSource() == &worldFileBrowse) {
        std::filesystem::path path;
        if (SelectFile({"pgw", "pngw", "jgw", "jpgw", "jpegw", "wld"}, ResourceText(3), path)) {
            SetPath(worldFileEdit, path);
            RefreshValidation();
        }
    } else if (event.GetSource() == &importButton) {
        PostCloseRequest(Accept);
    } else if (event.GetSource() == &cancelButton) {
        PostCloseRequest(Cancel);
    }
}

void ImportDialog::RadioItemChanged(const DG::RadioItemChangeEvent&)
{
    RefreshValidation();
}

void ImportDialog::TextEditChanged(const DG::TextEditChangeEvent& event)
{
    if (internalEdit) {
        return;
    }
    if (event.GetSource() == &rasterEdit) {
        AutoSelectWorldFile();
    }
    RefreshValidation();
}

bool ImportDialog::SelectFile(
    std::initializer_list<const char*> extensions,
    const GS::UniString& filter,
    std::filesystem::path& path
)
{
    FTM::FileTypeManager manager("GeoRasterFileTypes");
    if (extensions.size() == 0) {
        return false;
    }
    auto extension = extensions.begin();
    FTM::FileType type(nullptr, *extension, 0, 0, 0);
    for (++extension; extension != extensions.end(); ++extension) {
        type.AddExtension(*extension);
    }
    FTM::TypeID typeId = FTM::FileTypeManager::SearchForType(type);
    if (typeId == FTM::UnknownType) {
        typeId = manager.AddType(type);
    }
    DG::FileDialog dialog(DG::FileDialog::OpenFile);
    const UIndex filterIndex = dialog.AddFilter(typeId, DG::FileDialog::DisplayExtensions);
    dialog.SetFilterText(filterIndex, filter);
    if (!dialog.Invoke()) {
        return false;
    }
    GS::UniString selectedPath;
    dialog.GetSelectedFile().ToPath(&selectedPath);
    path = std::filesystem::path(GS::ToWString(selectedPath));
    return true;
}

void ImportDialog::AutoSelectWorldFile()
{
    discovery = GeoRaster::DiscoverWorldFiles(GetPath(rasterEdit));
    if (discovery.preferred) {
        SetPath(worldFileEdit, *discovery.preferred);
    } else {
        SetPath(worldFileEdit, {});
    }
}

void ImportDialog::RefreshValidation()
{
    importButton.Disable();
    const auto raster = GeoRaster::ReadRasterInfo(GetPath(rasterEdit));
    if (!raster.IsValid()) {
        previewText.SetText(ValidationMessage(raster.validation));
        return;
    }
    const auto worldFile = GeoRaster::ReadWorldFile(GetPath(worldFileEdit));
    if (!worldFile.IsValid()) {
        previewText.SetText(ValidationMessage(worldFile.validation));
        return;
    }

    const auto footprint = GeoRaster::ComputeWorldFootprint(
        *worldFile.value, raster.value->pixelWidth, raster.value->pixelHeight
    );
    const auto bounds = GeoRaster::ComputeBounds(footprint);
    const double width = bounds.maximum.x - bounds.minimum.x;
    const double height = bounds.maximum.y - bounds.minimum.y;
    GS::UniString preview = ResourceText(4) + ": " +
        GS::UniString::Printf("%u x %u px", raster.value->pixelWidth, raster.value->pixelHeight);
    preview += "\n" + ResourceText(5) + ": " + Number(worldFile.value->a, 6) + " x " +
               Number(std::abs(worldFile.value->e), 6) + " m/px";
    preview += "\n" + ResourceText(6) + ": [" + Number(bounds.minimum.x) + ", " +
               Number(bounds.minimum.y) + "] - [" + Number(bounds.maximum.x) + ", " +
               Number(bounds.maximum.y) + "]";
    preview += "\n" + ResourceText(7) + ": " + Number(width) + " x " + Number(height) + " m";
    if (discovery.candidates.size() > 1) {
        preview += "\n" + ResourceText(8) + ": " + GS::UniString::Printf(
            "%u", static_cast<unsigned int>(discovery.candidates.size())
        );
        for (const auto& candidate : discovery.candidates) {
            preview += " " + GS::ToUniString(candidate.filename().wstring());
        }
    }

    if (floorPlanRadio.IsSelected()) {
        if (!floorPlanAvailable || !projectToSurvey) {
            previewText.SetText(ResourceText(9));
            return;
        }
        preview += "\n" + ResourceText(39) + ": " + (
            projectLengthUnit ? GS::UniString(projectLengthUnit->symbol.c_str(), CC_UTF8)
                              : ResourceText(45)
        );
        const auto analysis = GeoRaster::AnalyzeFloorPlanPlacement(
            footprint, *projectToSurvey, projectLengthUnit.value_or(GeoRaster::LengthUnitInfo {})
        );
        preview += "\n" + ResourceText(40) + ": " +
                   ProjectPoint(analysis.diagnostics.surveyPointProjectPosition);
        preview += " (" + ResourceText(41) + ": " +
                   Number(analysis.diagnostics.surveyPointProjectPosition.x) + ", " +
                   Number(analysis.diagnostics.surveyPointProjectPosition.y) + " m)";
        preview += "\n" + ResourceText(42) + ": " +
                   Number(analysis.diagnostics.projectOriginSurveyCoordinates.x) + ", " +
                   Number(analysis.diagnostics.projectOriginSurveyCoordinates.y) + " m";
        if (!analysis.IsValid()) {
            preview += "\n" + ValidationMessage(analysis.validation);
            if (analysis.diagnostics.suggestedSurveyPointScale) {
                const double scale = *analysis.diagnostics.suggestedSurveyPointScale;
                const GeoRaster::Point2D suggested {
                    analysis.diagnostics.surveyPointProjectPosition.x * scale,
                    analysis.diagnostics.surveyPointProjectPosition.y * scale
                };
                preview += "\n" + ResourceText(44) + ": " + ProjectPoint(suggested) +
                           " (" + Number(suggested.x) + ", " + Number(suggested.y) + " m)";
            }
            previewText.SetText(preview);
            return;
        }
        preview += "\n" + ResourceText(10) + ": " + Number(analysis.placement->anchor.x) + ", " +
                   Number(analysis.placement->anchor.y) + " m; " + ResourceText(11) + ": " +
                   Number(analysis.placement->rotation * 180.0 / 3.14159265358979323846, 2) +
                   " deg";
    }
    preview += "\n" + ResourceText(12);
    previewText.SetText(preview);
    importButton.Enable();
}

void ImportDialog::SetPath(DG::TextEdit& edit, const std::filesystem::path& path)
{
    internalEdit = true;
    edit.SetText(GS::ToUniString(path.wstring()));
    internalEdit = false;
}

std::filesystem::path ImportDialog::GetPath(const DG::TextEdit& edit) const
{
    return std::filesystem::path(GS::ToWString(edit.GetText()));
}

GS::UniString ImportDialog::ProjectLength(double meters) const
{
    GS::UniString formatted;
    if (projectLengthUnit && ACCompat::FormatProjectLength(meters, formatted) == NoError) {
        return formatted;
    }
    return Number(meters) + " m";
}

GS::UniString ImportDialog::ProjectPoint(GeoRaster::Point2D point) const
{
    return ProjectLength(point.x) + ", " + ProjectLength(point.y);
}

GS::UniString ImportDialog::ValidationMessage(const GeoRaster::ValidationResult& validation) const
{
    short resourceIndex = 13;
    switch (validation.code) {
        case GeoRaster::ValidationCode::FileNotFound: resourceIndex = 13; break;
        case GeoRaster::ValidationCode::FileReadFailed: resourceIndex = 14; break;
        case GeoRaster::ValidationCode::InvalidWorldFileNumber: resourceIndex = 15; break;
        case GeoRaster::ValidationCode::InvalidWorldFileValueCount: resourceIndex = 16; break;
        case GeoRaster::ValidationCode::NonFiniteWorldFileValue: resourceIndex = 17; break;
        case GeoRaster::ValidationCode::ZeroPixelScale: resourceIndex = 18; break;
        case GeoRaster::ValidationCode::MirroredRaster: resourceIndex = 19; break;
        case GeoRaster::ValidationCode::RotatedOrShearedRaster: resourceIndex = 20; break;
        case GeoRaster::ValidationCode::UnsupportedRasterFormat: resourceIndex = 21; break;
        case GeoRaster::ValidationCode::CorruptPng: resourceIndex = 22; break;
        case GeoRaster::ValidationCode::CorruptJpeg: resourceIndex = 23; break;
        case GeoRaster::ValidationCode::NonFiniteSurveyTransform: resourceIndex = 24; break;
        case GeoRaster::ValidationCode::NonRigidSurveyTransform: resourceIndex = 25; break;
        case GeoRaster::ValidationCode::SingularSurveyTransform: resourceIndex = 26; break;
        case GeoRaster::ValidationCode::ProbableSurveyPointUnitMismatch:
            resourceIndex = 43;
            break;
        case GeoRaster::ValidationCode::TooFarFromProjectOrigin: resourceIndex = 27; break;
        default: resourceIndex = 28; break;
    }
    GS::UniString result = ResourceText(resourceIndex);
    for (const std::string& argument : validation.arguments) {
        result += " " + GS::UniString(argument.c_str(), CC_UTF8);
    }
    return result;
}

} // namespace GeoRasterUI
