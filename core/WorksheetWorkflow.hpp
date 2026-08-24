#pragma once

namespace GeoRaster {

template<typename Error>
struct WorksheetWorkflowResult {
    Error primary;
    Error restore;
    Error cleanup;
};

template<typename OriginalWindow, typename CreatedWorksheet, typename Error, typename Create,
         typename Activate, typename Insert, typename Restore, typename Remove>
WorksheetWorkflowResult<Error> RunWorksheetWorkflow(
    const OriginalWindow& original,
    Error success,
    Create&& create,
    Activate&& activate,
    Insert&& insert,
    Restore&& restore,
    Remove&& remove
)
{
    CreatedWorksheet created {};
    Error primary = create(created);
    if (primary != success) {
        return {primary, success, success};
    }

    primary = activate(created);
    if (primary == success) {
        primary = insert();
    }
    if (primary == success) {
        return {success, success, success};
    }

    const Error restoreError = restore(original);
    const Error cleanup = remove(created);
    return {primary, restoreError, cleanup};
}

} // namespace GeoRaster
