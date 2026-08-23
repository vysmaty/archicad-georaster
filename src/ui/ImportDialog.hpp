#pragma once

#include "core/GeoRasterCore.hpp"

#include "DGModule.hpp"

#include <filesystem>
#include <initializer_list>
#include <optional>

namespace GeoRasterUI {

enum class ImportTarget { NewWorksheet, ActiveFloorPlan };

struct ImportRequest {
    std::filesystem::path rasterPath;
    std::filesystem::path worldFilePath;
    ImportTarget target = ImportTarget::NewWorksheet;
};

class ImportDialog final : public DG::ModalDialog,
                           public DG::ButtonItemObserver,
                           public DG::RadioItemObserver,
                           public DG::TextEditBaseObserver {
public:
    ImportDialog(bool floorPlanAvailable, std::optional<GeoRaster::Affine2D> projectToSurvey);
    ~ImportDialog() override;

    [[nodiscard]] ImportRequest GetRequest() const;

private:
    enum ItemId : short {
        RasterEditId = 2,
        RasterBrowseId = 3,
        WorldFileEditId = 5,
        WorldFileBrowseId = 6,
        WorksheetRadioId = 7,
        FloorPlanRadioId = 8,
        PreviewTextId = 9,
        ImportButtonId = 10,
        CancelButtonId = 11
    };

    DG::Dialog& GetReference() { return *this; }
    void ButtonClicked(const DG::ButtonClickEvent& event) override;
    void RadioItemChanged(const DG::RadioItemChangeEvent& event) override;
    void TextEditChanged(const DG::TextEditChangeEvent& event) override;

    bool SelectFile(
        std::initializer_list<const char*> extensions,
        const GS::UniString& filter,
        std::filesystem::path& path
    );
    void AutoSelectWorldFile();
    void RefreshValidation();
    void SetPath(DG::TextEdit& edit, const std::filesystem::path& path);
    std::filesystem::path GetPath(const DG::TextEdit& edit) const;
    GS::UniString ValidationMessage(const GeoRaster::ValidationResult& validation) const;

    DG::TextEdit rasterEdit;
    DG::Button rasterBrowse;
    DG::TextEdit worldFileEdit;
    DG::Button worldFileBrowse;
    DG::RadioButton worksheetRadio;
    DG::RadioButton floorPlanRadio;
    DG::LeftText previewText;
    DG::Button importButton;
    DG::Button cancelButton;
    bool floorPlanAvailable;
    std::optional<GeoRaster::Affine2D> projectToSurvey;
    GeoRaster::WorldFileDiscovery discovery;
    bool internalEdit = false;
};

} // namespace GeoRasterUI
