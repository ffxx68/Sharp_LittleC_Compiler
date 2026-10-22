#include "codegen.h"
#include "parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static AstTranslationUnit *parse(AstArena *arena, const char *source)
{
    AstTranslationUnit *unit = NULL;
    assert(lcc2_parse_string(arena, source, &unit) == 0);
    assert(unit != NULL);
    return unit;
}

static void test_word_return_addition(void)
{
    static const char expected[] =
        "sum:\t; Procedure\n"
        "\tLIA\tLB(1)\t; Load word constant LB\n"
        "\tLIB\tHB(1)\t; Load word constant HB\n"
        "\tPUSH ; word (A, then B)\n"
        "\tEXAB\n"
        "\tPUSH\n"
        "\tLIA\tLB(2)\t; Load word constant LB\n"
        "\tLIB\tHB(2)\t; Load word constant HB\n"
        "\tLP\t0\n"
        "\tEXAM\n"
        "\tEXAB\n"
        "\tLP\t1\n"
        "\tEXAM\n"
        "\tPOP\n"
        "\tEXAB\n"
        "\tPOP\n"
        "\tADB\t; Addition\n"
        "\tLP\t1\n"
        "\tLDM\n"
        "\tEXAB\n"
        "\tLP\t0\n"
        "\tLDM\n"
        "\tRTN\n"
        "\n";
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit;
    CodegenContext *ctx = codegen_context_create();

    assert(arena != NULL && ctx != NULL);
    unit = parse(arena, "int sum(void) { return 1 + 2; }");
    assert(codegen_translation_unit(ctx, unit) == 0);
    assert(strcmp(codegen_output(ctx), expected) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    assert(codegen_error(ctx)[0] == '\0');
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_stack_and_constants(void)
{
    CodegenContext *ctx = codegen_context_create();
    char *label;

    assert(ctx != NULL);
    assert(codegen_pop_add(ctx, 1) != 0);
    assert(strcmp(codegen_error(ctx), "evaluation stack underflow") == 0);
    codegen_context_destroy(ctx);

    ctx = codegen_context_create();
    assert(ctx != NULL);
    assert(codegen_load_constant(ctx, 1, 255) == 0);
    assert(codegen_emit_push(ctx, 1) == 0);
    assert(codegen_stack_depth(ctx) == 1);
    assert(codegen_emit_pop(ctx, 1) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);

    ctx = codegen_context_create();
    assert(ctx != NULL);
    label = new_label(ctx);
    assert(label != NULL && strcmp(label, "LB0") == 0);
    emit_label(ctx, label);
    assert(strcmp(codegen_output(ctx), "  LB0:\n") == 0);
    free(label);
    codegen_context_destroy(ctx);
}

static void test_byte_arithmetic_sequences(void)
{
    static const char addition[] =
        "\tPUSH ; byte\n"
        "\tEXAB\n"
        "\tPOP\n"
        "\tLP\t3\n"
        "\tADM\t; Addition\n"
        "\tEXAB\n";
    static const char subtraction[] =
        "\tPUSH ; byte\n"
        "\tEXAB\n"
        "\tPOP\n"
        "\tLP\t3\n"
        "\tEXAB\n"
        "\tSBM\t; Subtraction\n"
        "\tEXAB\n";
    CodegenContext *ctx = codegen_context_create();

    assert(ctx != NULL);
    assert(codegen_emit_push(ctx, 1) == 0);
    assert(codegen_pop_add(ctx, 1) == 0);
    assert(strcmp(codegen_output(ctx), addition) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);

    ctx = codegen_context_create();
    assert(ctx != NULL);
    assert(codegen_emit_push(ctx, 1) == 0);
    assert(codegen_pop_sub(ctx, 1) == 0);
    assert(strcmp(codegen_output(ctx), subtraction) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
}

static void test_word_subtraction_sequence(void)
{
    static const char expected[] =
        "\tPUSH ; word (A, then B)\n"
        "\tEXAB\n"
        "\tPUSH\n"
        "\tLP\t1\n"
        "\tEXAM\n"
        "\tPOP\n"
        "\tEXAM\n"
        "\tLP\t0\n"
        "\tEXAM\n"
        "\tPOP\n"
        "\tEXAM\n"
        "\tSBB\t; Subtraction\n"
        "\tLP\t1\n"
        "\tLDM\n"
        "\tEXAB\n"
        "\tLP\t0\n"
        "\tLDM\n";
    CodegenContext *ctx = codegen_context_create();

    assert(ctx != NULL);
    assert(codegen_emit_push(ctx, 2) == 0);
    assert(codegen_pop_sub(ctx, 2) == 0);
    assert(strcmp(codegen_output(ctx), expected) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
}

static void test_unsupported_unit_is_not_partially_emitted(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit;
    CodegenContext *ctx = codegen_context_create();

    assert(arena != NULL && ctx != NULL);
    unit = parse(arena,
        "int supported(void) { return 1; } "
        "int unsupported(int value) { return value; }");
    assert(codegen_translation_unit(ctx, unit) != 0);
    assert(codegen_output(ctx)[0] == '\0');
    assert(strstr(codegen_error(ctx), "unsupported translation unit") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

int main(void)
{
    test_word_return_addition();
    test_stack_and_constants();
    test_byte_arithmetic_sequences();
    test_word_subtraction_sequence();
    test_unsupported_unit_is_not_partially_emitted();
    return 0;
}
