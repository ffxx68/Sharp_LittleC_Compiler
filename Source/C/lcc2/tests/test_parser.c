#include "parser.h"

#include <assert.h>

static void test_expression_precedence(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstFunctionDecl *function;
    AstReturnStmt *return_statement;
    AstBinaryExpr *sum;
    AstBinaryExpr *product;
    AstCompoundStmt *body;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "int global; int add(int a, int b) { return a + b * 2; }",
        &unit) == 0);
    assert(unit != NULL);
    assert(unit->declarations.count == 2);
    assert(unit->declarations.items[0]->node.kind == AST_NODE_VARIABLE_DECL);
    assert(unit->declarations.items[1]->node.kind == AST_NODE_FUNCTION_DECL);

    function = (AstFunctionDecl *)unit->declarations.items[1];
    assert(function->parameter_count == 2);
    assert(function->body != NULL);
    body = function->body;
    assert(body->item_count == 1);
    assert(body->items[0].kind == AST_BLOCK_STATEMENT);
    assert(body->items[0].as.statement->node.kind == AST_NODE_RETURN_STMT);
    return_statement = (AstReturnStmt *)body->items[0].as.statement;
    assert(return_statement->expression->node.kind == AST_NODE_BINARY_EXPR);
    sum = (AstBinaryExpr *)return_statement->expression;
    assert(sum->op == AST_BINARY_ADD);
    assert(sum->left->node.kind == AST_NODE_IDENT_EXPR);
    assert(sum->right->node.kind == AST_NODE_BINARY_EXPR);
    product = (AstBinaryExpr *)sum->right;
    assert(product->op == AST_BINARY_MUL);
    assert(product->left->node.kind == AST_NODE_IDENT_EXPR);
    assert(product->right->node.kind == AST_NODE_CONSTANT_EXPR);
    ast_arena_destroy(arena);
}

static void test_ternary_expression_ast(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstFunctionDecl *function;
    AstReturnStmt *return_statement;
    AstTernaryExpr *outer;
    AstTernaryExpr *inner;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "int choose(int a, int b, int c) { return a ? b : c ? 4 : 5; }",
        &unit) == 0);
    assert(unit != NULL && unit->declarations.count == 1);
    function = (AstFunctionDecl *)unit->declarations.items[0];
    return_statement = (AstReturnStmt *)function->body->items[0].as.statement;
    assert(return_statement->expression->node.kind == AST_NODE_TERNARY_EXPR);
    outer = (AstTernaryExpr *)return_statement->expression;
    assert(outer->condition->node.kind == AST_NODE_IDENT_EXPR);
    assert(outer->then_expression->node.kind == AST_NODE_IDENT_EXPR);
    assert(outer->else_expression->node.kind == AST_NODE_TERNARY_EXPR);
    inner = (AstTernaryExpr *)outer->else_expression;
    assert(inner->condition->node.kind == AST_NODE_IDENT_EXPR);
    assert(inner->then_expression->node.kind == AST_NODE_CONSTANT_EXPR);
    assert(inner->else_expression->node.kind == AST_NODE_CONSTANT_EXPR);
    ast_arena_destroy(arena);
}

static void test_declarators_and_statements(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstVarDecl *pointer;
    AstFunctionDecl *function;
    AstCompoundStmt *body;
    AstForStmt *loop;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "int *pointer; int *pointer_array[2]; int run(void) { int i = 0; "
        "for (i = 0; i < 3; i++) { if (i) continue; } return i; }",
        &unit) == 0);
    assert(unit->declarations.count == 3);
    pointer = (AstVarDecl *)unit->declarations.items[0];
    assert(pointer->type->kind == AST_TYPE_POINTER);
    assert(pointer->type->as.pointer.target->kind == AST_TYPE_BASE);

    pointer = (AstVarDecl *)unit->declarations.items[1];
    assert(pointer->type->kind == AST_TYPE_ARRAY);
    assert(pointer->type->as.array.element->kind == AST_TYPE_POINTER);
    assert(pointer->type->as.array.element->as.pointer.target->kind ==
           AST_TYPE_BASE);

    function = (AstFunctionDecl *)unit->declarations.items[2];
    body = function->body;
    assert(body->item_count == 3);
    assert(body->items[1].as.statement->node.kind == AST_NODE_FOR_STMT);
    loop = (AstForStmt *)body->items[1].as.statement;
    assert(loop->body->node.kind == AST_NODE_COMPOUND_STMT);
    ast_arena_destroy(arena);
}

static void test_initializers_attributes_and_casts(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstVarDecl *buffer;
    AstVarDecl *fixed;
    AstFunctionDecl *function;
    AstForStmt *loop;
    AstReturnStmt *return_statement;
    AstSizeofExpr *size_expression;
    AstCastExpr *cast;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "unsigned char buffer[3] = {1, 2, 3}; "
        "__at(123) int fixed; "
        "int run(void) { int i; for (i = 0; i < 1; i++) { } "
        "return (int)sizeof(int); }",
        &unit) == 0);
    assert(unit->declarations.count == 3);
    buffer = (AstVarDecl *)unit->declarations.items[0];
    assert(buffer->type->kind == AST_TYPE_ARRAY);
    assert(buffer->type->as.array.element->kind == AST_TYPE_BASE);
    assert(buffer->initializer->kind == AST_INITIALIZER_LIST);
    assert(buffer->initializer->as.list.count == 3);

    fixed = (AstVarDecl *)unit->declarations.items[1];
    assert(fixed->attributes.has_address);
    assert(fixed->attributes.address->node.kind == AST_NODE_CONSTANT_EXPR);
    function = (AstFunctionDecl *)unit->declarations.items[2];
    assert(function->parameter_count == 0);
    assert(function->body->item_count == 3);
    assert(function->body->items[0].kind == AST_BLOCK_DECLARATION);
    assert(function->body->items[1].as.statement->node.kind == AST_NODE_FOR_STMT);
    loop = (AstForStmt *)function->body->items[1].as.statement;
    assert(loop->init_declaration == NULL);
    return_statement =
    (AstReturnStmt *)function->body->items[2].as.statement;
    assert(return_statement->expression->node.kind == AST_NODE_CAST_EXPR);
    cast = (AstCastExpr *)return_statement->expression;
    assert(cast->operand->node.kind == AST_NODE_SIZEOF_EXPR);
    size_expression = (AstSizeofExpr *)cast->operand;
    assert(size_expression->is_type_operand);
    ast_arena_destroy(arena);
}

static void test_aggregate_types(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstVarDecl *pair;
    AstVarDecl *state;
    AstStructDecl *structure;
    AstEnumDecl *enumeration;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "struct Pair { int x, y; char tag; } pair; "
        "enum State { OFF, ON = 1 } state;",
        &unit) == 0);
    assert(unit->declarations.count == 2);
    pair = (AstVarDecl *)unit->declarations.items[0];
    assert(pair->type->kind == AST_TYPE_STRUCT);
    structure = pair->type->as.structure.declaration;
    assert(structure->is_complete);
    assert(structure->member_count == 2);
    assert(structure->members[0].declarator_count == 2);

    state = (AstVarDecl *)unit->declarations.items[1];
    assert(state->type->kind == AST_TYPE_ENUM);
    enumeration = state->type->as.enumeration.declaration;
    assert(enumeration->is_complete);
    assert(enumeration->enumerator_count == 2);
    assert(enumeration->enumerators[1]->value->node.kind ==
           AST_NODE_CONSTANT_EXPR);
    ast_arena_destroy(arena);
}

static void test_literal_values(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstVarDecl *binary;
    AstVarDecl *character;
    AstVarDecl *string;
    AstConstantExpr *integer;
    AstConstantExpr *character_constant;
    AstStringExpr *string_constant;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "int binary = 0b101u; char character = '\\n'; "
        "char *text = \"a\\n\" \"b\";",
        &unit) == 0);
    assert(unit->declarations.count == 3);
    binary = (AstVarDecl *)unit->declarations.items[0];
    integer = (AstConstantExpr *)binary->initializer->as.expression;
    assert(integer->value.integer == 5);

    character = (AstVarDecl *)unit->declarations.items[1];
    character_constant =
        (AstConstantExpr *)character->initializer->as.expression;
    assert(character_constant->value.character == '\n');

    string = (AstVarDecl *)unit->declarations.items[2];
    string_constant = (AstStringExpr *)string->initializer->as.expression;
    assert(string_constant->length == 3);
    assert(string_constant->bytes[0] == 'a');
    assert(string_constant->bytes[1] == '\n');
    assert(string_constant->bytes[2] == 'b');
    ast_arena_destroy(arena);
}

static void test_reject_unsupported_type(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena, "long value;", &unit) != 0);
    assert(unit == NULL);
    assert(lcc2_parse_string(arena, "int values[2][2];", &unit) != 0);
    assert(unit == NULL);
    assert(lcc2_parse_string(arena,
        "int function(void) { return 0; int late; }", &unit) != 0);
    assert(unit == NULL);
    ast_arena_destroy(arena);
}

static void test_typedef_classification_and_scope(void)
{
    AstArena *arena = ast_arena_create();
    AstTranslationUnit *unit = NULL;
    AstTypedefDecl *alias;
    AstVarDecl *value;
    AstFunctionDecl *function;
    AstVarDecl *local;
    AstVarDecl *global_local;

    assert(arena != NULL);
    assert(lcc2_parse_string(arena,
        "typedef unsigned char byte; byte value; "
        "int function(void) { typedef int local_type; "
        "local_type local; return local; } int local_type;",
        &unit) == 0);
    assert(unit->declarations.count == 4);
    alias = (AstTypedefDecl *)unit->declarations.items[0];
    assert(alias->declaration.node.kind == AST_NODE_TYPEDEF_DECL);
    value = (AstVarDecl *)unit->declarations.items[1];
    assert(value->type->kind == AST_TYPE_BASE);
    assert(value->type->as.base.base == AST_TYPE_CHAR);
    assert(value->type->as.base.is_unsigned);

    function = (AstFunctionDecl *)unit->declarations.items[2];
    assert(function->body->item_count == 3);
    assert(function->body->items[0].as.declaration->node.kind ==
           AST_NODE_TYPEDEF_DECL);
    local = (AstVarDecl *)function->body->items[1].as.declaration;
    assert(local->type->kind == AST_TYPE_BASE);
    assert(local->type->as.base.base == AST_TYPE_INT);
    global_local = (AstVarDecl *)unit->declarations.items[3];
    assert(global_local->declaration.node.kind == AST_NODE_VARIABLE_DECL);
    ast_arena_destroy(arena);
}

int main(void)
{
    test_expression_precedence();
    test_ternary_expression_ast();
    test_declarators_and_statements();
    test_initializers_attributes_and_casts();
    test_aggregate_types();
    test_literal_values();
    test_reject_unsupported_type();
    test_typedef_classification_and_scope();
    return 0;
}
