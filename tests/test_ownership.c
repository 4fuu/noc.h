#include "test_support.h"

static void test_move_borrow_return_and_drop(void)
{
    static const char source[] =
        "typedef struct Resource Resource;\n"
        "Resource *acquire(void);\n"
        "void release(Resource *resource);\n"
        "void observe(Resource *resource);\n"
        "Resource *transfer(void) {\n"
        "    own(Resource *, release) first = acquire();\n"
        "    observe(borrow(first));\n"
        "    own(Resource *, release) second = move(first);\n"
        "    observe(borrow(second));\n"
        "    return move(second);\n"
        "}\n"
        "void local(void) {\n"
        "    own(Resource *, release) resource = acquire();\n"
        "    observe(borrow(resource));\n"
        "}\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_OWNERSHIP, true));
    CHECK(noc_feature_is_enabled(&context, NOC_FEATURE_OWNERSHIP));
    CHECK(noc_feature_is_enabled(&context, NOC_FEATURE_DEFER));
    CHECK(noc_transform_source(&context,
                               "ownership.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "own(") == NULL);
    CHECK(strstr(result.output, "borrow(") == NULL);
    CHECK(strstr(result.output, "move(") == NULL);
    CHECK(strstr(result.output, "defer {") == NULL);
    CHECK(strstr(result.output, "first = 0") != NULL);
    CHECK(strstr(result.output, "second = 0") != NULL);
    CHECK(strstr(result.output, "noc_move_result_") != NULL);
    CHECK(strstr(result.output, "release(first)") != NULL);
    CHECK(strstr(result.output, "release(second)") != NULL);
    CHECK(strstr(result.output, "release(resource)") != NULL);
    check_complete_generated_c("generated-ownership.c",
                               result.output,
                               result.output_count);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_move_errors_and_feature_dependency(void)
{
    static const char moved_use[] =
        "typedef struct R R; R *make(void); void drop_r(R *); void see(R *);\n"
        "void bad(void) {\n"
        "  own(R *, drop_r) first = make();\n"
        "  own(R *, drop_r) second = move(first);\n"
        "  see(borrow(first));\n"
        "  see(borrow(second));\n"
        "}\n";
    static const char implicit_use[] =
        "typedef struct R R; R *make(void); void drop_r(R *); void see(R *);\n"
        "void bad(void) { own(R *, drop_r) item = make(); see(item); }\n";
    static const char non_pointer[] =
        "void drop_int(int value);\n"
        "void bad(void) { own(int, drop_int) value = 1; }\n";
    static const char implicit_copy[] =
        "typedef struct R R; R *make(void); void drop_r(R *);\n"
        "void bad(void) {\n"
        "  own(R *, drop_r) first = make();\n"
        "  own(R *, drop_r) second = first;\n"
        "}\n";
    static const char shadow_move[] =
        "typedef struct R R; R *make(void); void drop_r(R *);\n"
        "void bad(void) {\n"
        "  own(R *, drop_r) item = make();\n"
        "  { own(R *, drop_r) item = move(item); }\n"
        "}\n";
    static const char abstract_pointer[] =
        "void drop_callback(int (*callback)(void));\n"
        "void bad(void) { own(int (*)(void), drop_callback) callback = 0; }\n";
    static const char forward_goto[] =
        "typedef struct R R; R *make(void); void drop_r(R *);\n"
        "void bad(void) { goto done; own(R *, drop_r) item = make(); done:; }\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_OWNERSHIP, true));
    CHECK(!noc_set_feature_enabled(&context, NOC_FEATURE_DEFER, false));
    CHECK(strstr(diagnostics.last_message, "ownership") != NULL);
    CHECK(!noc_transform_source(&context,
                                "moved-use.c",
                                moved_use,
                                sizeof(moved_use) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "unavailable owner") != NULL ||
          strstr(diagnostics.last_message, "moved owner") != NULL);
    CHECK(!noc_transform_source(&context,
                                "implicit-use.c",
                                implicit_use,
                                sizeof(implicit_use) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "borrow() or move()") != NULL);
    CHECK(!noc_transform_source(&context,
                                "non-pointer.c",
                                non_pointer,
                                sizeof(non_pointer) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "pointer_type") != NULL);
    CHECK(!noc_transform_source(&context,
                                "implicit-copy.c",
                                implicit_copy,
                                sizeof(implicit_copy) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "must be transferred") != NULL);
    CHECK(!noc_transform_source(&context,
                                "shadow-move.c",
                                shadow_move,
                                sizeof(shadow_move) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "shadow-move") != NULL);
    CHECK(!noc_transform_source(&context,
                                "abstract-pointer.c",
                                abstract_pointer,
                                sizeof(abstract_pointer) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "declaration-prefix") != NULL);
    CHECK(!noc_transform_source(&context,
                                "forward-goto-owner.c",
                                forward_goto,
                                sizeof(forward_goto) - 1,
                                &result));
    CHECK(result.output == NULL);
    CHECK(strstr(diagnostics.last_message, "goto/break/continue") != NULL);
    noc_context_deinit(&context);
}

int main(void)
{
    test_move_borrow_return_and_drop();
    test_move_errors_and_feature_dependency();
    return finish_suite("ownership");
}
