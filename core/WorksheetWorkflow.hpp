#pragma once

namespace GeoRaster {

template<typename Error>
struct WorksheetWorkflowResult {
    Error primary;
    Error restore;
    Error cleanup;
};

template<typename Database, typename Error, typename Create, typename Change, typename Insert,
         typename Remove>
WorksheetWorkflowResult<Error> RunWorksheetWorkflow(
    const Database& original,
    Error success,
    Create&& create,
    Change&& change,
    Insert&& insert,
    Remove&& remove
)
{
    Database created {};
    Error primary = create(created);
    if (primary != success) {
        return {primary, success, success};
    }

    primary = change(created);
    if (primary == success) {
        primary = insert();
    }
    if (primary == success) {
        return {success, success, success};
    }

    Database originalCopy = original;
    const Error restore = change(originalCopy);
    const Error cleanup = remove(created);
    return {primary, restore, cleanup};
}

} // namespace GeoRaster
