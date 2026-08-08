#include "test_support.h"

#define CHECK_STATUS(expression, expected)                                      \
    do {                                                                        \
        Noc_Workspace_Status status__ = (expression);                           \
        CHECK(status__ == (expected));                                          \
    } while (0)

static void test_status_and_empty_handles(void)
{
    static const char *expected[] = {
        "ok",
        "invalid argument",
        "already open",
        "not current",
        "not found",
        "out of range",
        "out of memory",
        "limit exceeded",
    };
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Slice source;
    size_t index;
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        CHECK(strcmp(noc_workspace_status_name((Noc_Workspace_Status)index),
                     expected[index]) == 0);
    }
    CHECK(strcmp(noc_workspace_status_name((Noc_Workspace_Status)99),
                 "unknown workspace status") == 0);
    noc_workspace_init(&workspace);
    CHECK(workspace.impl == NULL);
    CHECK(!noc_document_snapshot_is_valid(&snapshot));
    CHECK(noc_document_snapshot_file_id(&snapshot) == NOC_FILE_ID_NONE);
    CHECK(noc_document_snapshot_generation(&snapshot) == 0);
    CHECK(noc_document_snapshot_path(&snapshot) == NULL);
    source = noc_document_snapshot_source(&snapshot);
    CHECK(source.data == NULL && source.count == 0);
    CHECK(noc_document_snapshot_source_class(&snapshot) == NOC_SOURCE_CLASS_PROJECT);
    CHECK(!noc_document_snapshot_is_current(&workspace, &snapshot));
    CHECK_STATUS(noc_document_snapshot_clone(&snapshot, &snapshot),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_workspace_deinit(&workspace);
}

static void test_physical_locations(void)
{
    static const char source[] = "a\r\nb\rc\n";
    static const size_t lines[] = {1, 1, 1, 2, 2, 3, 3, 4};
    static const size_t columns[] = {1, 2, 3, 1, 2, 1, 2, 1};
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Location location;
    Noc_Location preserved = {"preserved", 91, 92, 93};
    size_t offset;
    size_t scalar;
    noc_workspace_init(&workspace);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "memory://physical.c",
                                             source,
                                             sizeof(source) - 1,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &snapshot),
                 NOC_WORKSPACE_OK);
    for (offset = 0; offset <= sizeof(source) - 1; ++offset) {
        scalar = SIZE_MAX;
        CHECK_STATUS(noc_document_snapshot_location(&snapshot, offset, &location),
                     NOC_WORKSPACE_OK);
        CHECK(location.offset == offset);
        CHECK(location.line == lines[offset]);
        CHECK(location.column == columns[offset]);
        CHECK(strcmp(location.path, "memory://physical.c") == 0);
        CHECK_STATUS(noc_document_snapshot_offset(&snapshot,
                                                  location.line,
                                                  location.column,
                                                  &scalar),
                     NOC_WORKSPACE_OK);
        CHECK(scalar == offset);
    }
    location = preserved;
    CHECK_STATUS(noc_document_snapshot_location(&snapshot,
                                                sizeof(source),
                                                &location),
                 NOC_WORKSPACE_OUT_OF_RANGE);
    CHECK(location.path == preserved.path && location.offset == preserved.offset &&
          location.line == preserved.line && location.column == preserved.column);
    scalar = 123;
    CHECK_STATUS(noc_document_snapshot_offset(&snapshot, 0, 1, &scalar),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    CHECK(scalar == 123);
    CHECK_STATUS(noc_document_snapshot_offset(&snapshot, 5, 1, &scalar),
                 NOC_WORKSPACE_OUT_OF_RANGE);
    CHECK(scalar == 123);
    CHECK_STATUS(noc_document_snapshot_offset(&snapshot, 1, 4, &scalar),
                 NOC_WORKSPACE_OUT_OF_RANGE);
    CHECK(scalar == 123);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_shared_revision_update(void)
{
    static const char initial_source[] = "left\r\nright";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot expected = {0};
    Noc_Document_Snapshot output = {0};
    Noc_Slice source;
    Noc_Location location;
    size_t offset = SIZE_MAX;
    noc_workspace_init(&workspace);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "memory://shared.c",
                                             initial_source,
                                             sizeof(initial_source) - 1,
                                             NOC_SOURCE_CLASS_TRUSTED,
                                             &expected),
                 NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_document_snapshot_clone(&expected, &output), NOC_WORKSPACE_OK);
    CHECK(expected.impl == output.impl);
    source = noc_document_snapshot_source(&expected);
    CHECK_STATUS(noc_workspace_update_document(&workspace,
                                               &expected,
                                               source.data + 6,
                                               source.count - 6,
                                               &output),
                 NOC_WORKSPACE_OK);
    CHECK(expected.impl != output.impl);
    CHECK(noc_document_snapshot_generation(&expected) == 1);
    CHECK(noc_document_snapshot_generation(&output) == 2);
    CHECK(slice_equals(noc_document_snapshot_source(&expected), initial_source));
    CHECK(slice_equals(noc_document_snapshot_source(&output), "right"));
    CHECK(!noc_document_snapshot_is_current(&workspace, &expected));
    CHECK(noc_document_snapshot_is_current(&workspace, &output));

    noc_workspace_deinit(&workspace);
    CHECK_STATUS(noc_document_snapshot_location(&expected,
                                                sizeof(initial_source) - 1,
                                                &location),
                 NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_document_snapshot_offset(&expected,
                                              location.line,
                                              location.column,
                                              &offset),
                 NOC_WORKSPACE_OK);
    CHECK(offset == sizeof(initial_source) - 1);
    CHECK_STATUS(noc_document_snapshot_location(&output, 5, &location),
                 NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_document_snapshot_offset(&output,
                                              location.line,
                                              location.column,
                                              &offset),
                 NOC_WORKSPACE_OK);
    CHECK(offset == 5);
    noc_document_snapshot_free(&output);
    noc_document_snapshot_free(&expected);
}

static void test_snapshot_lifecycle(void)
{
    static const char initial_source[] = "first\r\nsecond\rthird\n";
    static const char binary_source[] = {'x', '\0', 'y'};
    Noc_Workspace workspace = {0};
    Noc_Workspace other_workspace = {0};
    Noc_Document_Snapshot current = {0};
    Noc_Document_Snapshot old = {0};
    Noc_Document_Snapshot lookup = {0};
    Noc_Document_Snapshot preserved = {0};
    Noc_Document_Snapshot foreign = {0};
    Noc_Document_Snapshot binary = {0};
    Noc_Document_Snapshot third = {0};
    Noc_Document_Snapshot_Impl *preserved_pointer;
    Noc_File_Id file_id;
    Noc_Slice source;
    size_t offset = 0;

    noc_workspace_init(&workspace);
    noc_workspace_init(&other_workspace);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "src/main.c",
                                             initial_source,
                                             sizeof(initial_source) - 1,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &current),
                 NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_is_valid(&current));
    file_id = noc_document_snapshot_file_id(&current);
    CHECK(file_id != NOC_FILE_ID_NONE);
    CHECK(noc_document_snapshot_generation(&current) == 1);
    CHECK(strcmp(noc_document_snapshot_path(&current), "src/main.c") == 0);
    CHECK(noc_document_snapshot_source_class(&current) == NOC_SOURCE_CLASS_PROJECT);
    source = noc_document_snapshot_source(&current);
    CHECK(source.count == sizeof(initial_source) - 1);
    CHECK(memcmp(source.data, initial_source, source.count) == 0);
    CHECK(noc_document_snapshot_is_current(&workspace, &current));

    CHECK_STATUS(noc_document_snapshot_clone(&current, &old), NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_document_snapshot_clone(&current, &preserved), NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_document_snapshot_clone(&current, &current), NOC_WORKSPACE_OK);
    preserved_pointer = preserved.impl;
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "src/main.c",
                                             "ignored",
                                             sizeof("ignored") - 1,
                                             NOC_SOURCE_CLASS_TRUSTED,
                                             &preserved),
                 NOC_WORKSPACE_ALREADY_OPEN);
    CHECK(preserved.impl == preserved_pointer);
    CHECK_STATUS(noc_workspace_find_document(&workspace, "./src/main.c", &preserved),
                 NOC_WORKSPACE_NOT_FOUND);
    CHECK(preserved.impl == preserved_pointer);
    CHECK_STATUS(noc_workspace_current_document(&workspace,
                                                NOC_FILE_ID_NONE,
                                                &preserved),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    CHECK(preserved.impl == preserved_pointer);

    source = noc_document_snapshot_source(&current);
    CHECK_STATUS(noc_workspace_update_document(&workspace,
                                               &current,
                                               source.data + 7,
                                               source.count - 7,
                                               &current),
                 NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_generation(&current) == 2);
    CHECK(noc_document_snapshot_file_id(&current) == file_id);
    CHECK(noc_document_snapshot_source_class(&current) == NOC_SOURCE_CLASS_PROJECT);
    CHECK(slice_equals(noc_document_snapshot_source(&current), "second\rthird\n"));
    CHECK(slice_equals(noc_document_snapshot_source(&old), initial_source));
    CHECK(!noc_document_snapshot_is_current(&workspace, &old));
    CHECK(noc_document_snapshot_is_current(&workspace, &current));
    CHECK_STATUS(noc_workspace_update_document(&workspace,
                                               &old,
                                               "stale",
                                               sizeof("stale") - 1,
                                               &preserved),
                 NOC_WORKSPACE_NOT_CURRENT);
    CHECK(preserved.impl == preserved_pointer);
    CHECK_STATUS(noc_workspace_close_document(&workspace, &old),
                 NOC_WORKSPACE_NOT_CURRENT);

    CHECK_STATUS(noc_workspace_current_document(&workspace, file_id, &lookup),
                 NOC_WORKSPACE_OK);
    CHECK(lookup.impl == current.impl);
    CHECK_STATUS(noc_workspace_find_document(&workspace, "src/main.c", &lookup),
                 NOC_WORKSPACE_OK);
    CHECK(lookup.impl == current.impl);

    CHECK_STATUS(noc_workspace_open_document(&other_workspace,
                                             "src/main.c",
                                             "foreign",
                                             sizeof("foreign") - 1,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &foreign),
                 NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_file_id(&foreign) == file_id);
    CHECK_STATUS(noc_workspace_update_document(&workspace,
                                               &foreign,
                                               "foreign update",
                                               sizeof("foreign update") - 1,
                                               &preserved),
                 NOC_WORKSPACE_NOT_CURRENT);
    CHECK(preserved.impl == preserved_pointer);

    CHECK_STATUS(noc_workspace_close_document(&workspace, &current),
                 NOC_WORKSPACE_OK);
    CHECK(!noc_document_snapshot_is_current(&workspace, &current));
    CHECK_STATUS(noc_workspace_find_document(&workspace, "src/main.c", &preserved),
                 NOC_WORKSPACE_NOT_FOUND);
    CHECK(preserved.impl == preserved_pointer);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "src/main.c",
                                             NULL,
                                             0,
                                             NOC_SOURCE_CLASS_SYSTEM,
                                             &current),
                 NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_file_id(&current) == file_id);
    CHECK(noc_document_snapshot_generation(&current) == 3);
    CHECK(noc_document_snapshot_source_class(&current) == NOC_SOURCE_CLASS_SYSTEM);
    CHECK(noc_document_snapshot_source(&current).count == 0);
    CHECK_STATUS(noc_document_snapshot_location(&current, 0, &(Noc_Location){0}),
                 NOC_WORKSPACE_OK);

    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "generated://binary.h",
                                             binary_source,
                                             sizeof(binary_source),
                                             NOC_SOURCE_CLASS_GENERATED,
                                             &binary),
                 NOC_WORKSPACE_OK);
    source = noc_document_snapshot_source(&binary);
    CHECK(source.count == sizeof(binary_source));
    CHECK(memcmp(source.data, binary_source, sizeof(binary_source)) == 0);
    CHECK(noc_document_snapshot_file_id(&binary) == file_id + 1);
    CHECK_STATUS(noc_document_snapshot_offset(&binary, 1, 4, &offset),
                 NOC_WORKSPACE_OK);
    CHECK(offset == sizeof(binary_source));

    CHECK_STATUS(noc_workspace_close_document(&workspace, &current),
                 NOC_WORKSPACE_OK);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "src/third.c",
                                             "third",
                                             sizeof("third") - 1,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &third),
                 NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_file_id(&third) ==
          noc_document_snapshot_file_id(&binary) + 1);

    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "",
                                             NULL,
                                             0,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &preserved),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "invalid.c",
                                             NULL,
                                             1,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             &preserved),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    CHECK_STATUS(noc_workspace_open_document(&workspace,
                                             "invalid.c",
                                             "",
                                             0,
                                             (Noc_Source_Class)99,
                                             &preserved),
                 NOC_WORKSPACE_INVALID_ARGUMENT);
    CHECK(preserved.impl == preserved_pointer);

    noc_workspace_deinit(&workspace);
    CHECK(workspace.impl == NULL);
    CHECK(noc_document_snapshot_is_valid(&current));
    CHECK(strcmp(noc_document_snapshot_path(&current), "src/main.c") == 0);
    CHECK(noc_document_snapshot_generation(&current) == 3);
    CHECK(noc_document_snapshot_source(&current).count == 0);
    CHECK(noc_document_snapshot_is_valid(&old));
    CHECK(slice_equals(noc_document_snapshot_source(&old), initial_source));

    noc_document_snapshot_free(&binary);
    noc_document_snapshot_free(&third);
    noc_document_snapshot_free(&foreign);
    noc_document_snapshot_free(&preserved);
    noc_document_snapshot_free(&lookup);
    noc_document_snapshot_free(&old);
    noc_document_snapshot_free(&current);
    noc_workspace_deinit(&other_workspace);
}

int main(void)
{
    (void)count_diagnostics;
    test_status_and_empty_handles();
    test_physical_locations();
    test_shared_revision_update();
    test_snapshot_lifecycle();
    return finish_suite("workspace");
}
