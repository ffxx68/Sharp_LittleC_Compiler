#include "ast.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    AstArena *arena;
    AstBinaryExpr *expression;
    AstConstantExpr *constant;
    char *copy;

    arena = ast_arena_create();
    assert(arena != NULL);

    expression = (AstBinaryExpr *)ast_arena_alloc(arena, sizeof(*expression));
    constant = (AstConstantExpr *)ast_arena_alloc(arena, sizeof(*constant));
    assert(expression != NULL);
    assert(constant != NULL);
    assert(expression->expression.node.kind == 0);
    assert(expression->left == NULL);
    assert(constant->type == NULL);

    expression->expression.node.kind = AST_NODE_BINARY_EXPR;
    expression->op = AST_BINARY_ADD;
    expression->left = (AstExpr *)constant;
    assert(expression->expression.node.kind == AST_NODE_BINARY_EXPR);
    assert(expression->op == AST_BINARY_ADD);
    assert(expression->left == (AstExpr *)constant);

    copy = ast_arena_strdup(arena, "identifier");
    assert(copy != NULL);
    assert(strcmp(copy, "identifier") == 0);
    assert(strcmp(ast_arena_strndup(arena, "source.c", 6), "source") == 0);
    assert(strcmp(ast_arena_strndup(arena, "short", 20), "short") == 0);
    assert(ast_arena_strdup(arena, NULL) == NULL);
    assert(ast_arena_strndup(arena, NULL, 3) == NULL);
    assert(ast_arena_alloc(NULL, sizeof(*expression)) == NULL);

    ast_arena_destroy(arena);
    ast_arena_destroy(NULL);
    return 0;
}
