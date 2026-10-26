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
        "int main(void) { float x; return 0; }",
        "int main(void) { return 65536; }",
        "unsigned char __at(8) value; int main(void) { &1; return 0; }",
        "unsigned char __at(8) value; int main(void) { int local; &local; return 0; }",
        "unsigned char __at(8) values[2]; int main(void) { &values; return 0; }",
        "unsigned char __at(8) values[2]; int main(void) { unsigned int i = 0; &values[i]; return 0; }",
        "unsigned char __at(8) value; int main(void) { (&value) + 1; return 0; }",
        "unsigned char __at(8) value; int main(void) { return &value; }",
        "unsigned char __at(8) value; int main(void) { if (&value) return 1; return 0; }",
        "int f(void) { return 0; } int main(void) { &f; return 0; }",
        "unsigned char __at(8) value; int main(void) { int *p; p = &value; return 0; }",
        "unsigned char __at(8) value; int main(void) { signed char *p; p = &value; return 0; }",
        "unsigned char __at(8) value; int main(void) { unsigned char *p; p = 1; return 0; }",
        "unsigned char __at(8) value; unsigned char __at(10) *p = &value;"
            " int main(void) { return 0; }",
        "unsigned char __at(8) value; int take(int n) { return n; }"
            " int main(void) { return take(&value); }",
        "unsigned char __at(8) value; int take(unsigned char *p) { return 0; }"
            " int main(void) { return take(&value[0]); }",
        "unsigned char __at(8) value; unsigned char *take(void) { return &value; }",
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

static void test_address_resolution(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) byte_value;"
        "unsigned int __at(10) word_value;"
        "unsigned char __at(12) byte_array[3];"
        "__xram __at(0xE600) unsigned char xram_byte;"
        "__xram __at(0xE602) unsigned int xram_word;"
        "__xram __at(0xE604) unsigned int word_array[2];"
        "void main(void) { &byte_value; &word_value; &xram_byte; &xram_word;"
        " &byte_array[2]; &word_array[1]; }");
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[6];
    AstAddressExpr *byte_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[0].as.statement)->expression;
    AstAddressExpr *word_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[1].as.statement)->expression;
    AstAddressExpr *xram_byte_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[2].as.statement)->expression;
    AstAddressExpr *xram_word_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[3].as.statement)->expression;
    AstAddressExpr *byte_element_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[4].as.statement)->expression;
    AstAddressExpr *word_element_address = (AstAddressExpr *)
        ((AstExprStmt *)function->body->items[5].as.statement)->expression;
    ResolvedAddress resolved;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_address_info(table, byte_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_CHAR);
    assert(resolved.pointer_width == 2);
    assert(resolved.location.storage == SCALAR_REGISTER);
    assert(resolved.location.address == 8 && resolved.location.width == 1);
    assert(symtab_address_info(table, word_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_INT);
    assert(resolved.pointer_width == 2);
    assert(resolved.location.storage == SCALAR_REGISTER);
    assert(resolved.location.address == 10 && resolved.location.width == 2);
    assert(symtab_address_info(table, xram_byte_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_CHAR);
    assert(resolved.location.storage == SCALAR_XRAM);
    assert(resolved.location.address == 0xE600 && resolved.location.width == 1);
    assert(symtab_address_info(table, xram_word_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_INT);
    assert(resolved.location.storage == SCALAR_XRAM);
    assert(resolved.location.address == 0xE602 && resolved.location.width == 2);
    assert(symtab_address_info(table, byte_element_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_CHAR);
    assert(resolved.location.storage == SCALAR_REGISTER);
    assert(resolved.location.address == 14 && resolved.location.width == 1);
    assert(symtab_address_info(table, word_element_address, &resolved) == 0);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_INT);
    assert(resolved.location.storage == SCALAR_XRAM);
    assert(resolved.location.address == 0xE606 && resolved.location.width == 2);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_pointer_values_and_parameters(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) byte_value;"
        "unsigned int __at(10) word_value;"
        "__xram __at(0xE610) unsigned char xram_value;"
        "unsigned char __at(12) *stored_pointer;"
        "__xram __at(0xE5FF) unsigned char *remote_pointer;"
        "unsigned int use_pointer(unsigned char *input, unsigned int *output) {"
        " unsigned char *copy = input; copy = input; return 0; }"
        "unsigned int relay(unsigned char *input) {"
        " return use_pointer(&xram_value, &word_value); }"
        "unsigned int main(void) {"
        " unsigned char *first = &byte_value; unsigned char *second;"
        " second = first; stored_pointer = second; remote_pointer = stored_pointer;"
        " relay(remote_pointer); use_pointer(0, &word_value); return 0; }");
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[5];
    AstVarDecl *copy = (AstVarDecl *)function->body->items[0].as.declaration;
    const ScalarSymbol *input;
    const ScalarSymbol *output;

    assert(symtab_resolve(table, unit) == 0);
    input = symtab_parameter(table, function->parameters[0]);
    output = symtab_parameter(table, function->parameters[1]);
    assert(input != NULL && input->location.width == 2);
    assert(output != NULL && output->location.width == 2);
    assert(input->location.address == 5 && output->location.address == 3);
    assert(symtab_declaration(table, copy)->location.width == 2);
    assert(symtab_declaration(table, copy)->location.storage == SCALAR_LOCAL);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[3])->location.address == 12);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[4])->location.address == 0xE5FF);
    assert(symtab_declaration(table,
        (AstVarDecl *)unit->declarations.items[4])->location.width == 2);
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_dereference_resolution(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) byte_value;"
        "__xram __at(0xE5FF) unsigned int word_value;"
        "unsigned int read(unsigned char *byte_pointer,"
        " unsigned int *word_pointer) {"
        " *byte_pointer = 1; return *word_pointer; }"
        "void main(void) { unsigned char *p = &byte_value;"
        " *p; *(&word_value); }");
    AstFunctionDecl *read_function =
        (AstFunctionDecl *)unit->declarations.items[2];
    AstBinaryExpr *assignment = (AstBinaryExpr *)
        ((AstExprStmt *)read_function->body->items[0].as.statement)->expression;
    AstDereferenceExpr *byte_dereference = (AstDereferenceExpr *)assignment->left;
    AstReturnStmt *return_statement =
        (AstReturnStmt *)read_function->body->items[1].as.statement;
    AstDereferenceExpr *word_dereference =
        (AstDereferenceExpr *)return_statement->expression;
    AstFunctionDecl *main_function =
        (AstFunctionDecl *)unit->declarations.items[3];
    AstDereferenceExpr *local_dereference = (AstDereferenceExpr *)
        ((AstExprStmt *)main_function->body->items[1].as.statement)->expression;
    AstDereferenceExpr *xram_dereference = (AstDereferenceExpr *)
        ((AstExprStmt *)main_function->body->items[2].as.statement)->expression;
    ResolvedDereference resolved;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_dereference_info(table, byte_dereference, &resolved) == 0);
    assert(resolved.width == 1 && resolved.is_unsigned);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_CHAR);
    assert(symtab_dereference_info(table, word_dereference, &resolved) == 0);
    assert(resolved.width == 2 && resolved.is_unsigned);
    assert(resolved.pointee_type->as.base.base == AST_TYPE_INT);
    assert(symtab_dereference_info(table, local_dereference, &resolved) == 0);
    assert(resolved.width == 1 && resolved.is_unsigned);
    assert(symtab_dereference_info(table, xram_dereference, &resolved) == 0);
    assert(resolved.width == 2 && resolved.is_unsigned);
    symtab_destroy(table);
    ast_arena_destroy(arena);
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

static void test_array_layout_and_binding(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned char __at(8) bytes[3] = {1, 2};"
        "__xram __at(0xE600) unsigned int words[2];"
        "unsigned int main(void) {"
        " unsigned char local[3] = {4, 5};"
        " unsigned int values[2];"
        " local[2] = bytes[1]; values[1] = words[0];"
        " return local[2] + values[1]; }");
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[2];
    AstVarDecl *local = (AstVarDecl *)function->body->items[0].as.declaration;
    AstVarDecl *values = (AstVarDecl *)function->body->items[1].as.declaration;
    AstExpr *assignment = ((AstExprStmt *)function->body->items[2].as.statement)->expression;
    AstIndexExpr *destination = (AstIndexExpr *)((AstBinaryExpr *)assignment)->left;
    ScalarLocation location;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_frame_size(table, function) == 7);
    assert(symtab_declaration(table, local)->is_array);
    assert(symtab_declaration(table, local)->array_length == 3);
    assert(symtab_declaration(table, values)->allocation_width == 4);
    assert(symtab_index_location(table, destination, &location) == 0);
    assert(location.storage == SCALAR_LOCAL && location.width == 1);
    assert(location.address == 4);
    {
        AstExpr *source = ((AstBinaryExpr *)assignment)->right;
        AstIndexExpr *source_index = (AstIndexExpr *)source;
        assert(symtab_index_location(table, source_index, &location) == 0);
        assert(location.storage == SCALAR_REGISTER && location.address == 9);
    }
    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_structure_layout(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "struct Point { unsigned char x; signed int y, z; } __at(8) point;"
        "__xram __at(0xE600) struct Packet { unsigned int word; "
        "unsigned char tag; } packet;"
        "int main(void) { struct Local { char first; int second; } local;"
        " return 0; }");
    AstVarDecl *point = (AstVarDecl *)unit->declarations.items[0];
    AstStructDecl *point_type = point->type->as.structure.declaration;
    AstVarDecl *packet = (AstVarDecl *)unit->declarations.items[1];
    AstStructDecl *packet_type = packet->type->as.structure.declaration;
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[2];
    AstVarDecl *local = (AstVarDecl *)function->body->items[0].as.declaration;
    AstStructDecl *local_type = local->type->as.structure.declaration;
    const ScalarSymbol *symbol;
    ResolvedStructMember member;
    size_t size;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_structure_size(table, point_type, &size) == 0 && size == 5);
    assert(symtab_structure_member(table, point_type, "x", &member) == 0);
    assert(member.structure == point_type && member.declaration != NULL);
    assert(member.type->as.base.base == AST_TYPE_CHAR);
    assert(member.offset == 0 && member.width == 1);
    assert(symtab_structure_member(table, point_type, "y", &member) == 0);
    assert(member.type->as.base.base == AST_TYPE_INT);
    assert(!member.type->as.base.is_unsigned);
    assert(member.offset == 1 && member.width == 2);
    assert(symtab_structure_member(table, point_type, "z", &member) == 0);
    assert(member.offset == 3 && member.width == 2);
    assert(symtab_structure_member(table, point_type, "missing", &member) != 0);
    assert(symtab_structure_size(table, packet_type, &size) == 0 && size == 3);
    symbol = symtab_declaration(table, point);
    assert(symbol != NULL && symbol->is_aggregate);
    assert(symbol->allocation_width == 5 && symbol->location.width == 5);
    assert(symbol->location.storage == SCALAR_REGISTER &&
           symbol->location.address == 8);
    symbol = symtab_declaration(table, packet);
    assert(symbol != NULL && symbol->is_aggregate);
    assert(symbol->allocation_width == 3);
    assert(symbol->location.storage == SCALAR_XRAM &&
           symbol->location.address == 0xE600);
    assert(symtab_structure_size(table, local_type, &size) == 0 && size == 3);
    symbol = symtab_declaration(table, local);
    assert(symbol != NULL && symbol->is_aggregate);
    assert(symbol->allocation_width == 3);
    assert(symbol->location.storage == SCALAR_LOCAL &&
           symbol->location.address == -2);
    assert(symtab_frame_size(table, function) == 3);

    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_direct_structure_member_resolution(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "struct Point { unsigned char x; signed int y; } __at(8) point;"
        "int main(void) {"
        " struct Local { unsigned char first; signed int second; } local;"
        " local.first = 5; return local.second; }"
        "int use_point(void) { point.x = 7; return point.y; }");
    AstVarDecl *object = (AstVarDecl *)unit->declarations.items[0];
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[1];
    AstBinaryExpr *assignment = (AstBinaryExpr *)
        ((AstExprStmt *)function->body->items[1].as.statement)->expression;
    AstVarDecl *local = (AstVarDecl *)function->body->items[0].as.declaration;
    AstMemberExpr *local_byte = (AstMemberExpr *)assignment->left;
    AstMemberExpr *local_word = (AstMemberExpr *)
        ((AstReturnStmt *)function->body->items[2].as.statement)->expression;
    AstFunctionDecl *global_function =
        (AstFunctionDecl *)unit->declarations.items[2];
    AstBinaryExpr *global_assignment = (AstBinaryExpr *)
        ((AstExprStmt *)global_function->body->items[0].as.statement)->expression;
    AstMemberExpr *x_member = (AstMemberExpr *)global_assignment->left;
    AstMemberExpr *y_member = (AstMemberExpr *)
        ((AstReturnStmt *)global_function->body->items[1].as.statement)->expression;
    ResolvedMemberAccess access;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_member_access(table, x_member, &access) == 0);
    assert(access.member.name != NULL && strcmp(access.member.name, "x") == 0);
    assert(access.location.storage == SCALAR_REGISTER);
    assert(access.location.address == 8 && access.location.width == 1);
    assert(access.location.is_unsigned && access.is_modifiable);
    assert(symtab_member_access(table, y_member, &access) == 0);
    assert(access.location.storage == SCALAR_REGISTER);
    assert(access.location.address == 9 && access.location.width == 2);
    assert(!access.location.is_unsigned && access.is_modifiable);
    assert(symtab_member_access(table, local_byte, &access) == 0);
    assert(access.location.storage == SCALAR_LOCAL);
    assert(access.location.address == -2 && access.location.width == 1);
    assert(symtab_declaration(table, local)->is_aggregate);
    assert(symtab_member_access(table, local_word, &access) == 0);
    assert(access.location.storage == SCALAR_LOCAL);
    assert(access.location.address == -1 && access.location.width == 2);
    assert(symtab_identifier(table, x_member->object) != NULL);
    assert(symtab_identifier(table, x_member->object)->declaration == object);

    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_pointer_structure_member_resolution(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "int inspect(struct Pair { unsigned char tag; signed int value; } *p) {"
        " p->value = 7; return p->tag; }"
        "struct Local { unsigned char first; unsigned int second; }"
        " __at(8) object;"
        "int main(void) { return (&object)->second; }");
    AstFunctionDecl *inspect = (AstFunctionDecl *)unit->declarations.items[0];
    AstBinaryExpr *assignment = (AstBinaryExpr *)
        ((AstExprStmt *)inspect->body->items[0].as.statement)->expression;
    AstMemberExpr *write = (AstMemberExpr *)assignment->left;
    AstMemberExpr *read = (AstMemberExpr *)
        ((AstReturnStmt *)inspect->body->items[1].as.statement)->expression;
    AstVarDecl *global_object = (AstVarDecl *)unit->declarations.items[1];
    AstFunctionDecl *main_function =
        (AstFunctionDecl *)unit->declarations.items[2];
    AstMemberExpr *address_read = (AstMemberExpr *)
        ((AstReturnStmt *)main_function->body->items[0].as.statement)->expression;
    ResolvedMemberAccess access;
    const ScalarSymbol *parameter;

    assert(symtab_resolve(table, unit) == 0);
    parameter = symtab_parameter(table, inspect->parameters[0]);
    assert(parameter != NULL && parameter->location.width == 2);
    assert(symtab_member_access(table, write, &access) == 0);
    assert(access.is_indirect && access.pointer_expression == write->object);
    assert(access.member.offset == 1 && access.member.width == 2);
    assert(access.location.is_unsigned == 0 && access.is_modifiable);
    assert(symtab_member_access(table, read, &access) == 0);
    assert(access.is_indirect && access.member.offset == 0);
    assert(access.member.width == 1 && access.location.is_unsigned);
    assert(symtab_member_access(table, address_read, &access) == 0);
    assert(access.is_indirect && access.member.offset == 1);
    assert(symtab_declaration(table, global_object)->is_aggregate);

    symtab_destroy(table);
    ast_arena_destroy(arena);
}

static void test_structure_rejections(void)
{
    static const char *sources[] = {
        "struct Item item;",
        "union Value { char byte; int word; } value;",
        "struct Duplicate { char value; int value; } object;",
        "struct PointerField { int *value; } object;",
        "struct ArrayField { int value[2]; } object;",
        "struct Nested { struct Inner { char value; } inner; } object;",
        "struct Floating { float value; } object;",
        "struct Initialized { char value; } object = {1};",
        "struct TooLarge { char first; char second; } __at(95) object;",
        "struct First { int value; } __at(8) first;"
            "struct Second { char a; char b; } __at(9) second;",
        "struct Value { char byte; int word; } __at(8) value;"
            "int main(void) { return value; }",
        "struct Value { char byte; } __at(8) value;"
            "int main(void) { return value.missing; }",
        "struct Value { char byte; } __at(8) value;"
            "int main(void) { value->byte = 1; return 0; }",
        "int main(void) { const struct Value { char byte; } value;"
            " value.byte = 1; return 0; }",
        "struct Value { const char byte; } __at(8) value;"
            "int main(void) { value.byte = 1; return 0; }",
        "struct Value { char byte; } __at(8) value;"
            "int main(void) { int other; other.byte = 1; return 0; }",
        "struct Value { char byte; } __at(8) value;"
            "int main(void) { value.byte.byte = 1; return 0; }",
        "int main(void) { int *ptr; return ptr->value; }",
        "int main(void) { struct Value { char byte; } *ptr;"
            " return ptr->missing; }",
        "int read(const struct Value { char byte; } *ptr) {"
            " ptr->byte = 1; return 0; }",
        "int read(struct Value { char byte; } *ptr) {"
            " return ptr->byte.byte; }"
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

static void test_array_rejections(void)
{
    static const char *sources[] = {
        "int main(void) { int values[2]; return values[2]; }",
        "int main(void) { int values[2]; return values[-1]; }",
        "void index(void) {} int main(void) { int values[2]; return values[index()]; }",
        "int main(void) { int values[2]; return values[0 - 1]; }",
        "int main(void) { const int values[2] = {1}; values[0] = 2; return 0; }",
        "int main(void) { int values[0]; return 0; }",
        "int main(void) { int values[17]; return 0; }",
        "int main(void) { int n = 2; int values[n]; return 0; }",
        "int main(void) { int values[]; return 0; }",
        "int main(void) { int values[2] = {1, 2, 3}; return 0; }",
        "int main(void) { int values[2] = {{1}}; return 0; }",
        "int main(void) { char values[2] = \"a\"; return 0; }",
        "int main(void) { int values[2]; return values; }",
        "__xram __at(65534) int values[2]; void main(void) {}",
        "unsigned char __at(95) values[2]; void main(void) {}",
        "int main(void) { float values[2]; return 0; }",
        "int main(void) { int *values[2]; return 0; }",
        "int main(void) { int value = 1; return value[0]; }",
        "int main(void) { int values[2]; values = 1; return 0; }",
        "int main(void) { const int values[2] = {1}; int i = 0;"
            " values[i] = 2; return 0; }",
        "int main(void) { int values[2]; return 0; } "
            "unsigned int __at(8) collision[2]; "
            "unsigned char __at(9) overlap;"
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

static void test_dynamic_array_index_resolution(void)
{
    AstArena *arena = ast_arena_create();
    ScalarSymbols *table = symtab_create();
    AstTranslationUnit *unit = parse(arena,
        "unsigned int main(void) {"
        " unsigned int values[4]; unsigned int i = 0;"
        " values[i] = values[i + 1] + 1; return values[i]; }");
    AstFunctionDecl *function = (AstFunctionDecl *)unit->declarations.items[0];
    AstExpr *assignment = ((AstExprStmt *)function->body->items[2].as.statement)->expression;
    AstIndexExpr *destination = (AstIndexExpr *)((AstBinaryExpr *)assignment)->left;

    assert(symtab_resolve(table, unit) == 0);
    assert(symtab_identifier(table, destination->index) != NULL);
    assert(symtab_identifier(table, destination->index)->declaration != NULL);
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
    test_address_resolution();
    test_pointer_values_and_parameters();
    test_dereference_resolution();
    test_global_initializers();
    test_storage_namespaces();
    test_frame_limit();
    test_control_flow_resolution();
    test_parameter_layout_and_calls();
    test_array_layout_and_binding();
    test_structure_layout();
    test_direct_structure_member_resolution();
    test_pointer_structure_member_resolution();
    test_structure_rejections();
    test_array_rejections();
    test_dynamic_array_index_resolution();
    return 0;
}
