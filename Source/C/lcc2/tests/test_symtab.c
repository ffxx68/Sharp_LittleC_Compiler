#include "symtab.h"
#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AstTranslationUnit *parse(AstArena *arena, const char *source)
{
    AstTranslationUnit *unit = NULL;
    assert(arena != NULL);
    assert(lcc2_parse_string(arena, source, &unit) == 0);
    return unit;
}

static void test_scopes_and_layout(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) x;"
        "__xram __at(0xE1FF) unsigned int out;"
        "unsigned int main(void) {"
        " unsigned int x = 7;"
        " { unsigned char x = 2; out = x; }"
        " { unsigned char x = 3; out = x; }"
        " return x; }"
        "unsigned int next(void) { return x; }");
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[2];
    AstVarDecl *local = (AstVarDecl *)function->body->items[0].as.declaration;
    AstCompoundStmt *inner = (AstCompoundStmt *)function->body->items[1].as.statement;
    AstVarDecl *shadow = (AstVarDecl *)inner->items[0].as.declaration;
    AstBinaryExpr *assignment = (AstBinaryExpr *)
        ((AstExprStmt *)inner->items[1].as.statement)->expression;
    AstExpr *returned = ((AstReturnStmt *)function->body->items[3].as.statement)->expression;
    AstFunctionDecl *next = (AstFunctionDecl *)unit->declarations.items[3];
    const ScalarSymbol *symbol;

    assert(table != NULL);
    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_frame_size(table, function) == 4);
    symbol = symtab_declaration(table, local);
    assert(symbol->location.storage == SCALAR_LOCAL && symbol->location.width == 2);
    assert(symbol->location.address == 1);
    symbol = symtab_declaration(table, shadow);
    assert(symbol->location.address == -1 && symbol->location.width == 1);
    assert(symtab_identifier(table, assignment->right)->declaration == shadow);
    assert(symtab_identifier(table, returned)->declaration == local);
    symbol = symtab_identifier(table,
        ((AstReturnStmt *)next->body->items[0].as.statement)->expression);
    assert(symbol->location.storage == SCALAR_REGISTER && symbol->location.address == 8);
    assert(symtab_identifier(table, assignment->left)->location.storage == SCALAR_XRAM);
    assert(symtab_declaration(table, (AstVarDecl *)unit->declarations.items[1])->location.address == 0xE1FF);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_rejections(void)
{
    static const char *sources[] = {
        "int main(void) { return missing; }",
        "int main(void) { int x; int x; return 0; }",
        "int main(void) { { int x; } return x; }",
        "int main(void) { int x; return X; }",
        "int main(void) { const int x = 1; x = 2; return x; }",
        "int main(void) { 1 = 2; return 0; }",
        "int main(void) { int x; return x * 2; }",
        "int main(void) { return later; } int __at(8) later;",
        "int __at(8) x; int __at(9) y;",
        "int __at(95) x;",
        "int __at(0) x;",
        "__xram __at(65535) int x;",
        "__xram int x;",
        "int x;",
        "int __at(8) x = y;",
        "int __at(8) x; void x(void) {}",
        "int main(void) { static int x; return x; }",
        "int main(void) { int __at(8) x; return x; }",
        "int main(void) { int x[2]; return 0; }",
        "int main(void) { float x; return 0; }",
        "int main(void) { return 65536; }",
        "void main(void) { return 1; }",
        "int main(void) { return; }",
        "int main(void) {}",
        "int main(void) { return 0; ; }",
        "int main(void); unsigned int main(void) { return 0; }",
        "int f(int value); unsigned int f(unsigned int value) { return value; }",
        "int main(void) { return missing(1); }",
        "int f(int value) { return value; } int main(void) { return f(); }",
        "void f(void) {} int main(void) { return f(); }",
        "void f(void) {} int main(void) { if (f()) return 1; return 0; }"
    };
    size_t index;
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        AstArena *arena = ast_arena_create();
        ScalarSymbols *table = symtab_create();
        AstTranslationUnit *unit = parse(arena, sources[index]);
        assert(symtab_resolve(table, unit) != 0);
        assert(symtab_error(table)[0] != '\0');
        symtab_destroy(table);
        ast_arena_destroy(arena);
    }
}

static void test_parameter_layout_and_calls(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned int pair(unsigned int left, unsigned char right) {"
        " unsigned int local;"
        " local = left + right;"
        " return local; }"
        "unsigned int main(void) { return pair(1, 2); }");
    AstFunctionDecl *pair = (AstFunctionDecl *)unit->declarations.items[0];
    const ScalarSymbol *left, *right, *local;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_frame_size(table, pair) == 2);
    left = symtab_parameter(table, pair->parameters[0]);
    right = symtab_parameter(table, pair->parameters[1]);
    local = symtab_declaration(table,
        (AstVarDecl *)pair->body->items[0].as.declaration);
    assert(left != NULL && left->location.storage == SCALAR_LOCAL);
    assert(left->location.width == 2 && left->location.address == 4);
    assert(right != NULL && right->location.width == 1);
    assert(right->location.address == 2);
    assert(local != NULL && local->location.width == 2);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_global_initializers(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) byte_value = 0x123;"
        "unsigned char __at(9) character_value = 'A';"
        "unsigned char __at(10) zero_register;"
        "__xram __at(0xE600) signed int word_value = 0 - 1;"
        "__xram __at(0xE602) unsigned char zero_value;"
        "void main(void) {}");
    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[0])->location.address == 8);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[3])->location.storage == SCALAR_XRAM);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[4])->location.width == 1);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_storage_namespaces(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) x;"
        "unsigned char __at(9) X;"
        "__xram __at(8) unsigned int other;"
        "unsigned int main(void); unsigned int main(void) { return x + X; }");
    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_resolve(table, unit) != 0);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_frame_limit(void)
{
    size_t count;
    for (count = 16; count <= 17; ++count) {
        char source[1024] = "int main(void) {";
        size_t index;
        AstArena *arena = ast_arena_create();
        ScalarSymbols *table = symtab_create();
        AstTranslationUnit *unit;
        for (index = 0; index < count; ++index) {
            size_t length = strlen(source);
            (void)snprintf(source + length, sizeof(source) - length,
                           " %s value%u;", index == 16 ? "char" : "int", (unsigned)index);
        }
        strcat(source, " return 0; }");
        unit = parse(arena, source);
        if (count == 16) {
            assert(symtab_resolve(table, unit) == 0);
            assert(symtab_frame_size(table,
                (AstFunctionDecl *)unit->declarations.items[0]) == 32);
        } else {
            assert(symtab_resolve(table, unit) != 0);
            assert(strstr(symtab_error(table), "frame exceeds 32 bytes") != NULL);
        }
        symtab_destroy(table);
        ast_arena_destroy(arena);
    }
}

static void test_control_flow_resolution(void)
{
    static const char *invalid[] = {
        "void main(void) { break; }",
        "void main(void) { continue; }",
        "int main(void) { int value; if (value < 1) return 0; return 1; }"
    };
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned int main(void) {"
        " unsigned int i = 0;"
        " if (i == 0) i = i + 1; else i = i + 2;"
        " while (i < 4) {"
        "   if (i == 2) { i = i + 1; continue; }"
        "   i = i + 1;"
        " }"
        " do { i = i - 1; } while (i > 0);"
        " for (i = 0; i < 2; i = i + 1) { if (i != 0) break; }"
        " return i; }");
    size_t index;

    assert(symtab_resolve(table, unit) == 0);
    symtab_destroy(table);
    ast_arena_destroy(arena);

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        arena = ast_arena_create();
        table = symtab_create();
        unit = parse(arena, invalid[index]);
        assert(symtab_resolve(table, unit) != 0);
        assert(symtab_error(table)[0] != '\0');
        symtab_destroy(table);
        ast_arena_destroy(arena);
    }
}

int main(void)
{
    test_scopes_and_layout();
    test_rejections();
    test_global_initializers();
    test_storage_namespaces();
    test_frame_limit();
    test_control_flow_resolution();
    test_parameter_layout_and_calls();
    return 0;
}
