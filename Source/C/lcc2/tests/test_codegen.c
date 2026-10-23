#include "codegen.h"
#include "parser.h"

#include <assert.h>
#include <stdio.h>
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
        "\tLP\t0\n"
        "\tADB\t; Addition\n"
        "\tLP\t1\n"
        "\tLDM\n"
        "\tEXAB\n"
        "\tLP\t0\n"
        "\tLDM\n"
        "\tJP\tLB0\n"
        "  LB0:\n"
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
        "int unsupported(int *value) { return 1; }");
    assert(codegen_translation_unit(ctx, unit) != 0);
    assert(codegen_output(ctx)[0] == '\0');
    assert(strstr(codegen_error(ctx), "unsupported function parameter type") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_complete_program(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "#pragma org(0xE000 + 48)\n"
        "unsigned int main(void) { return 40 + 3 - 1; }");
    static const char startup[] =
        "\t; pasm file - assemble with pasm!\n"
        "\t; Compiled with lcc2\n"
        ".ORG\t57392\n\n"
        "\tLP\t0\n\tLIDP\tSREG\n\tLII\t11\n\tEXWD\n"
        "\tCALL\tmain\n"
        "\tLP\t0\n\tLIDP\tSREG\n\tLII\t11\n\tMVWD\n\tRTN\n";

    assert(codegen_program(ctx, unit) == 0);
    assert(strncmp(codegen_output(ctx), startup, strlen(startup)) == 0);
    assert(strstr(codegen_output(ctx), "main:\t; Procedure\n") != NULL);
    assert(strstr(codegen_output(ctx), "SREG:\t.DW") >
           strstr(codegen_output(ctx), "main:\t; Procedure\n"));
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);

    arena = ast_arena_create();
    ctx = codegen_context_create();
    unit = parse(arena, "#pragma nosave\nvoid main(void) {}");
    assert(codegen_program(ctx, unit) == 0);
    assert(strstr(codegen_output(ctx), ".ORG\t57392\n") != NULL);
    assert(strstr(codegen_output(ctx), "\tCALL\tmain\n\tRTN\n") != NULL);
    assert(strstr(codegen_output(ctx), "SREG") == NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_global_initialization(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) byte_value = 0x123;"
        "unsigned char __at(9) character_value = 'A';"
        "unsigned char __at(10) zero_register;"
        "__xram __at(0xE600) signed int word_value = 0 - 1;"
        "__xram __at(0xE602) unsigned char zero_value;"
        "void main(void) {}");
    const char *assembly;
    const char *byte_init, *character_init, *zero_register_init;
    const char *word_init, *zero_xram_init, *main_call;

    assert(codegen_program(ctx, unit) == 0);
    assembly = codegen_output(ctx);
    byte_init = strstr(assembly, "LIA\t35\t; Load byte constant 35");
    character_init = strstr(assembly, "LIA\t65\t; Load byte constant 65");
    zero_register_init = strstr(assembly, "LIA\t0\t; Load byte constant 0");
    word_init = strstr(assembly, "LIA\tLB(65535)\t; Load word constant LB");
    assert(byte_init != NULL && character_init != NULL && word_init != NULL &&
           zero_register_init != NULL);
    zero_xram_init = strstr(zero_register_init + 1,
                            "LIA\t0\t; Load byte constant 0");
    main_call = strstr(assembly, "CALL\tmain");
    assert(zero_xram_init != NULL);
    assert(main_call != NULL);
    assert(byte_init < character_init && character_init < zero_register_init &&
           zero_register_init < word_init && word_init < zero_xram_init &&
           zero_xram_init < main_call);
    assert(strstr(byte_init, "LP\t8\n\tEXAM\n") < character_init);
    assert(strstr(character_init, "LP\t9\n\tEXAM\n") < zero_register_init);
    assert(strstr(zero_register_init, "LP\t10\n\tEXAM\n") < word_init);
    assert(strstr(word_init, "LIDP\t58880\n\tSTD\n") < zero_xram_init);
    assert(strstr(zero_xram_init, "LIDP\t58882\n\tSTD\n") < main_call);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_invalid_programs(void)
{
    static const char *sources[] = {
        "",
        "int main(void);",
        "int helper(void) { return 1; }",
        "int main(int arg) { return 1; }",
        "int main(void) { return 1; } int main(void) { return 2; }",
        "void SREG(void) {} void main(void) {}",
        "void sreg(void) {} void main(void) {}",
        "void Main(void) {} void main(void) {}",
        "void LB0(void) {} void main(void) {}",
        "#pragma org(65536)\nvoid main(void) {}",
        "#pragma org(1 - 2)\nvoid main(void) {}",
        "#pragma org(1 * 2)\nvoid main(void) {}",
        "#pragma pc(57392)\nvoid main(void) {}",
        "int value; void main(void) {}"
    };
    size_t index;
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        AstArena *arena = ast_arena_create();
        CodegenContext *ctx = codegen_context_create();
        AstTranslationUnit *unit = parse(arena, sources[index]);
        assert(codegen_program(ctx, unit) != 0);
        assert(codegen_output(ctx)[0] == '\0');
        assert(codegen_error(ctx)[0] != '\0');
        codegen_context_destroy(ctx);
        ast_arena_destroy(arena);
    }
}

static void test_scalar_access_sequences(void)
{
    static const struct {
        ScalarLocation location;
        int store;
        const char *expected;
    } cases[] = {
        {{SCALAR_REGISTER, 1, 8, 1}, 0, "\tLP\t8\n\tLDM\n"},
        {{SCALAR_REGISTER, 1, 64, 1}, 1, "\tLIP\t64\n\tEXAM\n"},
        {{SCALAR_REGISTER, 2, 63, 1}, 0,
         "\tLIP\t64\n\tLDM\n\tEXAB\n\tDECP\n\tLDM\n"},
        {{SCALAR_REGISTER, 2, 63, 1}, 1,
         "\tLP\t63\n\tEXAM\n\tEXAB\n\tINCP\n\tEXAM\n"},
        {{SCALAR_XRAM, 1, 0xE500, 1}, 0, "\tLIDP\t58624\n\tLDD\n"},
        {{SCALAR_XRAM, 1, 0xE500, 1}, 1, "\tLIDP\t58624\n\tSTD\n"},
        {{SCALAR_XRAM, 2, 0xE500, 1}, 0,
         "\tLIDP\t58625\n\tLDD\n\tEXAB\n\tLIDL\t0\n\tLDD\n"},
        {{SCALAR_XRAM, 2, 0xE500, 1}, 1,
         "\tLIDP\t58624\n\tSTD\n\tEXAB\n\tLIDL\t1\n\tSTD\n"},
        {{SCALAR_XRAM, 2, 0xE5FF, 1}, 0,
         "\tLIDP\t58880\n\tLDD\n\tEXAB\n\tLIDP\t58879\n\tLDD\n"},
        {{SCALAR_XRAM, 2, 0xE5FF, 1}, 1,
         "\tLIDP\t58879\n\tSTD\n\tEXAB\n\tLIDP\t58880\n\tSTD\n"},
        {{SCALAR_LOCAL, 1, -2, 1}, 0, "\tLDR\n\tADIA\t0\n\tSTP\n\tLDM\n"},
        {{SCALAR_LOCAL, 1, -2, 1}, 1,
         "\tEXAB\n\tLDR\n\tADIA\t0\n\tSTP\n\tEXAB\n\tEXAM\n"},
        {{SCALAR_LOCAL, 2, -1, 1}, 0,
         "\tLDR\n\tADIA\t0\n\tSTP\n\tLDM\n\tEXAB\n\tINCP\n\tLDM\n"},
        {{SCALAR_LOCAL, 2, -1, 1}, 1,
         "\tPUSH ; byte\n\tLDR\n\tADIA\t2\n\tSTP\n\tPOP\n\tEXAM\n\tEXAB\n\tDECP\n\tEXAM\n"}
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        CodegenContext *ctx = codegen_context_create();
        assert(ctx != NULL);
        assert((cases[index].store ?
            codegen_store_scalar(ctx, cases[index].location) :
            codegen_load_scalar(ctx, cases[index].location)) == 0);
        assert(strcmp(codegen_output(ctx), cases[index].expected) == 0);
        assert(codegen_stack_depth(ctx) == 0);
        codegen_context_destroy(ctx);
    }
    {
        CodegenContext *ctx = codegen_context_create();
        ScalarLocation local = {SCALAR_LOCAL, 2, -1, 1};
        assert(codegen_emit_push(ctx, 2) == 0);
        assert(codegen_load_scalar(ctx, local) == 0);
        assert(strstr(codegen_output(ctx), "\tADIA\t2\n") != NULL);
        assert(codegen_store_scalar(ctx, local) == 0);
        assert(strstr(codegen_output(ctx), "\tADIA\t4\n") != NULL);
        assert(codegen_stack_depth(ctx) == 2);
        assert(codegen_emit_pop(ctx, 2) == 0);
        codegen_context_destroy(ctx);
    }
}

static void test_scalar_validation(void)
{
    static const ScalarLocation invalid[] = {
        {SCALAR_REGISTER, 2, 95, 1},
        {SCALAR_REGISTER, 1, -1, 1},
        {SCALAR_XRAM, 2, 65535, 1},
        {SCALAR_LOCAL, 2, -2, 1},
        {SCALAR_LOCAL, 1, 254, 1},
        {SCALAR_REGISTER, 8, 8, 1}
    };
    size_t index;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        CodegenContext *ctx = codegen_context_create();
        assert(codegen_load_scalar(ctx, invalid[index]) != 0);
        assert(codegen_output(ctx)[0] == '\0');
        codegen_context_destroy(ctx);
        ctx = codegen_context_create();
        assert(codegen_store_scalar(ctx, invalid[index]) != 0);
        assert(codegen_output(ctx)[0] == '\0');
        codegen_context_destroy(ctx);
    }
}

static void test_scalar_tree_walker(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "__xram __at(0xE500) unsigned int result;"
        "unsigned int main(void) {"
        " unsigned int x = 40; unsigned char b = 3;"
        " { unsigned int x = 7; result = x; }"
        " result = (b = 258) + x; return result; }");
    assert(codegen_program(ctx, unit) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    assert(strstr(codegen_output(ctx), "\tLIDP\t58624\n\tSTD\n") != NULL);
    assert(strstr(codegen_output(ctx), "\tADIA\t") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_array_access_and_initialization(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) bytes[3] = {1, 2};"
        "__xram __at(0xE600) unsigned int words[2] = {0x1234};"
        "unsigned int main(void) {"
        " unsigned char local[3] = {4, 5};"
        " unsigned int local_words[2] = {0x1234};"
        " local[2] = bytes[1]; words[1] = local[2];"
        " local_words[1] = words[1]; return local_words[1]; }");
    const char *assembly;

    assert(codegen_program(ctx, unit) == 0);
    assembly = codegen_output(ctx);
    assert(strstr(assembly, "\tLP\t8\n\tEXAM\n") != NULL);
    assert(strstr(assembly, "\tLP\t9\n\tLDM\n") != NULL);
    assert(strstr(assembly, "\tLIDP\t58882\n\tSTD\n") != NULL);
    assert(strstr(assembly, "LIA\tLB(4660)") != NULL);
    assert(strstr(assembly, "\tLDR\n\tADIA\t") != NULL);
    assert(strstr(assembly, "\tINCP\n\tLDM\n") != NULL);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);

    arena = ast_arena_create();
    ctx = codegen_context_create();
    unit = parse(arena,
        "void index(void) {} int main(void) { int values[2]; return values[index()]; }");
    assert(codegen_program(ctx, unit) != 0);
    assert(codegen_output(ctx)[0] == '\0');
    assert(strstr(codegen_error(ctx),
        "void function call used as a value") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_dynamic_array_access(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "__xram __at(0xE600) unsigned int result;"
        "unsigned int main(void) {"
        " unsigned int values[4] = {10, 20, 30, 40};"
        " unsigned int i = 2;"
        " values[i] = values[i + 0] + 1;"
        " result = values[i]; return result; }");
    const char *assembly;

    assert(codegen_program(ctx, unit) == 0);
    assembly = codegen_output(ctx);
    assert(strstr(assembly, "CALL\tLIB_CPE16") == NULL);
    assert(strstr(assembly, "\tSTP\n\tINCP\n\tPOP\n\tEXAB\n\tPOP\n") != NULL);
    assert(strstr(assembly, "\tEXAM\n\tLDM\n\tEXAB\n\tDECP\n\tEXAM\n\tLDM\n") != NULL);
    assert(strstr(assembly, "\tLDM\n\tEXAB\n\tINCP\n\tLDM\n") != NULL);
    assert(codegen_stack_depth(ctx) == 0);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_dynamic_array_extent_independence(void)
{
    static const char *sources[] = {
        "__xram __at(0xF000) unsigned int values[4];"
        "unsigned int fetch(unsigned int i) { return values[i + 1]; }",
        "__xram __at(0xF000) unsigned int values[1024];"
        "unsigned int fetch(unsigned int i) { return values[i + 1]; }"
    };
    AstArena *arenas[2];
    CodegenContext *contexts[2];
    size_t index;

    for (index = 0; index < 2; ++index) {
        AstTranslationUnit *unit;
        arenas[index] = ast_arena_create();
        contexts[index] = codegen_context_create();
        unit = parse(arenas[index], sources[index]);
        assert(codegen_translation_unit(contexts[index], unit) == 0);
        assert(codegen_stack_depth(contexts[index]) == 0);
        assert(codegen_runtime_output(contexts[index])[0] == '\0');
        assert(strstr(codegen_output(contexts[index]),
            "\tLP\t4\n\tEXAM\n\tEXAB\n\tLP\t5\n\tEXAM\n\tDX\n\tIXL\n"
            "\tEXAB\n\tIXL\n\tEXAB\n") != NULL);
    }
    assert(strcmp(codegen_output(contexts[0]), codegen_output(contexts[1])) == 0);
    for (index = 0; index < 2; ++index) {
        codegen_context_destroy(contexts[index]);
        ast_arena_destroy(arenas[index]);
    }
}

static void test_comparisons_and_control_flow(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned int main(void) {"
        " unsigned int i = 0;"
        " for (; i < 2; i = i + 3) {"
        "   i = i + 7;"
        "   if (i == 1) continue;"
        "   if (i != 0) break;"
        " }"
        " while (i >= 1) { i = i - 1; }"
        " if (i > 0) i = i - 1;"
        " do { i = i + 1; } while (i <= 2);"
        " return i; }");
    const char *condition;
    const char *body;
    const char *step;

    assert(codegen_translation_unit(ctx, unit) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPS16") != NULL);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPE16") != NULL);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPNE16") != NULL);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPG16") != NULL);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPGE16") != NULL);
    assert(strstr(codegen_output(ctx), "CALL\tLIB_CPSE16") != NULL);
    condition = strstr(codegen_output(ctx), "LIA\tLB(2)");
    body = strstr(codegen_output(ctx), "LIA\tLB(7)");
    step = strstr(codegen_output(ctx), "LIA\tLB(3)");
    assert(condition != NULL && body != NULL && step != NULL);
    assert(condition < body && body < step);
    assert(strstr(codegen_output(ctx), "JP\tLB") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_function_calls_parameters_and_frames(void)
{
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned int pair(unsigned int left, unsigned char right) {"
        " unsigned int result = left + right;"
        " return result; }"
        "unsigned int inner(unsigned int value) { return value + 1; }"
        "unsigned int outer(void) { return inner(inner(1)); }"
        "unsigned char narrow(unsigned int value) { return value; }"
        "void sink(unsigned char first, unsigned char second) {}"
        "void sink4(char a, char b, char c, char d) {}"
        "unsigned int main(void) {"
        " sink(1, 2); sink4(1, 2, 3, 4);"
        " return pair(outer(), narrow(258)); }");
    const char *assembly;
    const char *inner_call;
    const char *outer_call;
    const char *cleanup_two;
    const char *cleanup_four;
    const char *definition;

    assert(arena != NULL && ctx != NULL);
    assert(codegen_translation_unit(ctx, unit) == 0);
    assert(codegen_stack_depth(ctx) == 0);
    assembly = codegen_output(ctx);
    assert(strstr(assembly, "LDR\n\tADIA\t5\n\tSTP") != NULL);
    assert(strstr(assembly, "LDR\n\tADIA\t6\n\tSTP") != NULL);
    assert(strstr(assembly, "CALL\tinner\t; Function call") != NULL);
    assert(strstr(assembly, "CALL\tpair\t; Function call") != NULL);
    assert(strstr(assembly, "CALL\tnarrow\t; Function call") != NULL);
    assert(strstr(assembly, "CALL\tnarrow\t; Function call\n"
                            "\tEXAB\n\tLDR\n\tADIA\t2\n\tSTR\n\tEXAB\n"
                            "\tLIB\t0\n") != NULL);
    cleanup_two = strstr(assembly, "CALL\tsink\t; Function call");
    cleanup_four = strstr(assembly, "CALL\tsink4\t; Function call");
    assert(cleanup_two != NULL && cleanup_four != NULL);
    cleanup_two = strstr(cleanup_two, "\tPOP\n\tPOP\n");
    cleanup_four = strstr(cleanup_four, "\tLDR\n\tADIA\t4\n\tSTR\n");
    assert(cleanup_two != NULL && cleanup_four != NULL);
    inner_call = strstr(assembly, "CALL\tinner\t; Function call");
    outer_call = inner_call == NULL ? NULL :
        strstr(inner_call + 1, "CALL\tinner\t; Function call");
    assert(inner_call != NULL && outer_call != NULL);
    definition = strstr(assembly, "pair:\t; Procedure");
    assert(definition != NULL);
    assert(strstr(definition, "\tPUSH\n\tPUSH\n") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static size_t occurrences(const char *source, const char *text)
{
    size_t count = 0;
    while ((source = strstr(source, text)) != NULL) {
        ++count;
        source += strlen(text);
    }
    return count;
}

static void test_program_sections(void)
{
    static const char source[] =
        "unsigned int __at(32) seed = 40;"
        "__xram __at(0xE600) unsigned int result;"
        "unsigned int add(unsigned int x) { return x + 1; }"
        "void main(void) { if (seed < 42) result = add(add(seed)); }";
    AstArena *arena = ast_arena_create();
    CodegenContext *ctx = codegen_context_create();
    CodegenContext *again = codegen_context_create();
    AstTranslationUnit *unit = parse(arena, source);
    const char *startup, *functions, *data, *runtime, *composed;
    size_t offset = 0;
    int section;
    assert(codegen_program(ctx, unit) == 0);
    startup = codegen_section_output(ctx, CODEGEN_STARTUP);
    functions = codegen_section_output(ctx, CODEGEN_FUNCTIONS);
    data = codegen_section_output(ctx, CODEGEN_DATA);
    runtime = codegen_section_output(ctx, CODEGEN_RUNTIME);
    assert(strstr(startup, "LB(40)") < strstr(startup, "CALL\tmain"));
    assert(strstr(startup, "main:\t; Procedure") == NULL);
    assert(strstr(startup, "SREG:") == NULL);
    assert(strstr(functions, "add:\t; Procedure") != NULL);
    assert(strstr(functions, "main:\t; Procedure") != NULL);
    assert(strstr(functions, "LIB_CPS16:") == NULL);
    assert(strcmp(data, "SREG:\t.DW 0, 0, 0, 0, 0, 0\n\n") == 0);
    assert(occurrences(runtime, "LIB_CPS16:") == 1);
    composed = codegen_output(ctx);
    for (section = CODEGEN_STARTUP; section <= CODEGEN_RUNTIME; ++section) {
        const char *text = codegen_section_output(ctx, (CodegenSection)section);
        assert(strncmp(composed + offset, text, strlen(text)) == 0);
        offset += strlen(text);
    }
    assert(offset == strlen(composed));
    assert(codegen_program(again, unit) == 0);
    assert(strcmp(composed, codegen_output(again)) == 0);
    assert(codegen_program(ctx, unit) != 0);
    codegen_context_destroy(ctx);
    codegen_context_destroy(again);
    ast_arena_destroy(arena);

    arena = ast_arena_create();
    ctx = codegen_context_create();
    unit = parse(arena, "#pragma nosave\nvoid main(void) {}");
    assert(codegen_program(ctx, unit) == 0);
    assert(codegen_section_output(ctx, CODEGEN_DATA)[0] == '\0');
    assert(codegen_section_output(ctx, CODEGEN_RUNTIME)[0] == '\0');
    assert(strstr(codegen_section_output(ctx, CODEGEN_STARTUP),
                  "CALL\tmain\n\tRTN\n") != NULL);
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
}

static void test_runtime_libraries(void)
{
    static const char *operators[] = {"==", "!=", "<", "<=", ">", ">="};
    static const char *routines[] = {
        "LIB_CPE16", "LIB_CPNE16", "LIB_CPS16", "LIB_CPSE16",
        "LIB_CPG16", "LIB_CPGE16"
    };
    size_t index;
    for (index = 0; index < 6; ++index) {
        AstArena *arena = ast_arena_create();
        CodegenContext *ctx = codegen_context_create();
        char source[256], label[40];
        AstTranslationUnit *unit;
        (void)snprintf(source, sizeof(source),
            "unsigned int main(void) {"
            " unsigned int x = 1; x = x %s 2; return x %s 3; }",
            operators[index], operators[index]);
        unit = parse(arena, source);
        assert(codegen_program(ctx, unit) == 0);
        (void)snprintf(label, sizeof(label), "%s:", routines[index]);
        assert(occurrences(codegen_runtime_output(ctx), label) == 1);
        assert(occurrences(codegen_output(ctx), label) == 1);
        assert(strstr(codegen_output(ctx), label) >
               strstr(codegen_output(ctx), "main:\t; Procedure"));
        assert(strstr(codegen_runtime_output(ctx), ".include") == NULL);
        assert(codegen_stack_depth(ctx) == 0);
        codegen_context_destroy(ctx);
        ast_arena_destroy(arena);
    }
    {
        AstArena *arena = ast_arena_create();
        CodegenContext *ctx = codegen_context_create();
        AstTranslationUnit *unit = parse(arena, "void main(void) {}");
        assert(codegen_program(ctx, unit) == 0);
        assert(codegen_runtime_output(ctx)[0] == '\0');
        assert(strstr(codegen_output(ctx), "LIB_CP") == NULL);
        codegen_context_destroy(ctx);
        ctx = codegen_context_create();
        assert(codegen_require_runtime(ctx, "LIB_MISSING") != 0);
        assert(strstr(codegen_error(ctx), "unresolved runtime routine") != NULL);
        codegen_context_destroy(ctx);
        ast_arena_destroy(arena);
    }
    {
        AstArena *arena = ast_arena_create();
        CodegenContext *ctx = codegen_context_create();
        AstTranslationUnit *unit = parse(arena,
            "void cpq1(void) {} unsigned int main(void) { return 1 == 2; }");
        assert(codegen_program(ctx, unit) != 0);
        assert(strstr(codegen_error(ctx), "runtime library label") != NULL);
        codegen_context_destroy(ctx);
        ast_arena_destroy(arena);
    }
}

int main(void)
{
    test_word_return_addition();
    test_stack_and_constants();
    test_byte_arithmetic_sequences();
    test_word_subtraction_sequence();
    test_unsupported_unit_is_not_partially_emitted();
    test_complete_program();
    test_global_initialization();
    test_invalid_programs();
    test_scalar_access_sequences();
    test_scalar_validation();
    test_scalar_tree_walker();
    test_array_access_and_initialization();
    test_dynamic_array_access();
    test_dynamic_array_extent_independence();
    test_comparisons_and_control_flow();
    test_function_calls_parameters_and_frames();
    test_runtime_libraries();
    test_program_sections();
    return 0;
}
