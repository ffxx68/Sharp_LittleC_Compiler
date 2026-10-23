#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Binding {
    const AstExpr *expression;
    size_t symbol;
} Binding;

struct ScalarSymbols {
    ScalarSymbol *symbols;
    size_t count;
    Binding *bindings;
    size_t binding_count;
    const AstFunctionDecl *function;
    size_t scope;
    size_t local_bytes;
    size_t loop_depth;
    int returned;
    int resolved;
    char error[256];
};

static int fail(ScalarSymbols *table, const char *message, const char *name)
{
    if (table->error[0] == '\0') {
        (void)snprintf(table->error, sizeof(table->error), "%s%s%s",
                       message, name == NULL ? "" : ": ", name == NULL ? "" : name);
    }
    return -1;
}

ScalarSymbols *symtab_create(void)
{
    return calloc(1, sizeof(ScalarSymbols));
}

void symtab_destroy(ScalarSymbols *table)
{
    if (table != NULL) {
        free(table->symbols);
        free(table->bindings);
        free(table);
    }
}

const char *symtab_error(const ScalarSymbols *table)
{
    return table == NULL ? "out of memory creating symbol table" : table->error;
}

size_t symtab_type_width(const AstType *type)
{
    if (type == NULL || type->kind != AST_TYPE_BASE ||
        (type->qualifiers & ~AST_QUALIFIER_CONST) != 0) {
        return 0;
    }
    return type->as.base.base == AST_TYPE_CHAR ? 1 :
           type->as.base.base == AST_TYPE_INT ? 2 : 0;
}

int symtab_constant(const AstExpr *expression, unsigned long *value)
{
    if (expression == NULL) {
        return -1;
    }
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        if (constant->constant_kind == AST_CONSTANT_INTEGER &&
            constant->value.integer >= 0 && constant->value.integer <= 65535) {
            *value = (unsigned long)constant->value.integer;
            return 0;
        }
        return -1;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        unsigned long left, right;
        if (symtab_constant(binary->left, &left) != 0 ||
            symtab_constant(binary->right, &right) != 0) {
            return -1;
        }
        if (binary->op == AST_BINARY_ADD && left + right <= 65535) {
            *value = left + right;
            return 0;
        }
        if (binary->op == AST_BINARY_SUB && left >= right) {
            *value = left - right;
            return 0;
        }
    }
    return -1;
}

static int add_variable(ScalarSymbols *table, const AstVarDecl *variable)
{
    ScalarSymbol symbol;
    ScalarSymbol *grown;
    size_t index;
    unsigned long address;

    memset(&symbol, 0, sizeof(symbol));
    symbol.location.width = symtab_type_width(variable->type);
    if (symbol.location.width == 0 || variable->name == NULL ||
        variable->storage != AST_STORAGE_NONE) {
        return fail(table, "unsupported scalar declaration", variable->name);
    }
    for (index = 0; index < table->count; ++index) {
        const ScalarSymbol *other = &table->symbols[index];
        if (other->active && other->scope == table->scope &&
            strcmp(other->declaration->name, variable->name) == 0) {
            return fail(table, "duplicate scalar declaration", variable->name);
        }
    }
    symbol.declaration = variable;
    symbol.function = table->function;
    symbol.scope = table->scope;
    symbol.active = 1;
    symbol.location.is_unsigned = variable->type->as.base.is_unsigned;
    if (table->function != NULL) {
        if (variable->attributes.has_address || variable->attributes.is_xram) {
            return fail(table, "local storage attributes are not supported yet", variable->name);
        }
        table->local_bytes += symbol.location.width;
        if (table->local_bytes > 32) {
            return fail(table, "leaf scalar frame exceeds 32 bytes", variable->name);
        }
        symbol.location.storage = SCALAR_LOCAL;
        symbol.location.address = -(int)table->local_bytes -
                                  (symbol.location.width == 1 ? 2 : 1);
    } else {
        if (variable->initializer != NULL) {
            return fail(table, "global initialization is not supported yet", variable->name);
        }
        if (!variable->attributes.has_address ||
            symtab_constant(variable->attributes.address, &address) != 0) {
            return fail(table, "global scalars require a constant __at address", variable->name);
        }
        symbol.location.storage = variable->attributes.is_xram ? SCALAR_XRAM : SCALAR_REGISTER;
        if (symbol.location.storage == SCALAR_REGISTER &&
            (address < 8 || address + symbol.location.width > 96)) {
            return fail(table, "register scalar must fit in internal RAM [8, 96)", variable->name);
        }
        if (address + symbol.location.width > 65536) {
            return fail(table, "scalar address exceeds target memory", variable->name);
        }
        symbol.location.address = (int)address;
        for (index = 0; index < table->count; ++index) {
            const ScalarLocation *other = &table->symbols[index].location;
            if (other->storage == symbol.location.storage &&
                (int)address < other->address + (int)other->width &&
                other->address < (int)address + (int)symbol.location.width) {
                return fail(table, "overlapping scalar storage", variable->name);
            }
        }
    }
    grown = realloc(table->symbols, (table->count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return fail(table, "out of memory adding scalar", variable->name);
    }
    table->symbols = grown;
    table->symbols[table->count++] = symbol;
    return 0;
}

static int expression_unsigned_word(const ScalarSymbols *table,
                                    const AstExpr *expression);
static int expression_nonnegative(const ScalarSymbols *table,
                                  const AstExpr *expression);

static int resolve_expression(ScalarSymbols *table, const AstExpr *expression)
{
    if (expression == NULL) {
        return fail(table, "missing scalar expression", NULL);
    }
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        if ((constant->constant_kind == AST_CONSTANT_INTEGER &&
             constant->value.integer >= 0 && constant->value.integer <= 65535) ||
            (constant->constant_kind == AST_CONSTANT_CHARACTER &&
             constant->value.character <= 255)) {
            return 0;
        }
        return fail(table, "unsupported or out-of-range scalar constant", NULL);
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR) {
        const char *name = ((const AstIdentExpr *)expression)->name;
        size_t index = table->count;
        while (index != 0) {
            const ScalarSymbol *symbol = &table->symbols[--index];
            if (symbol->active && strcmp(symbol->declaration->name, name) == 0) {
                Binding *grown = realloc(table->bindings,
                    (table->binding_count + 1) * sizeof(*grown));
                if (grown == NULL) {
                    return fail(table, "out of memory binding scalar", name);
                }
                table->bindings = grown;
                grown[table->binding_count].expression = expression;
                grown[table->binding_count++].symbol = index;
                return 0;
            }
        }
        return fail(table, "undeclared scalar", name);
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op != AST_BINARY_ADD && binary->op != AST_BINARY_SUB &&
            binary->op != AST_BINARY_ASSIGN &&
            binary->op != AST_BINARY_LESS &&
            binary->op != AST_BINARY_LESS_EQUAL &&
            binary->op != AST_BINARY_GREATER &&
            binary->op != AST_BINARY_GREATER_EQUAL &&
            binary->op != AST_BINARY_EQUAL &&
            binary->op != AST_BINARY_NOT_EQUAL) {
            return fail(table, "unsupported scalar operator", NULL);
        }
        if (resolve_expression(table, binary->left) != 0 ||
            resolve_expression(table, binary->right) != 0) {
            return -1;
        }
        if ((binary->op == AST_BINARY_LESS ||
             binary->op == AST_BINARY_LESS_EQUAL ||
             binary->op == AST_BINARY_GREATER ||
             binary->op == AST_BINARY_GREATER_EQUAL) &&
            !expression_unsigned_word(table, binary->left) &&
            !expression_unsigned_word(table, binary->right) &&
            !(expression_nonnegative(table, binary->left) &&
              expression_nonnegative(table, binary->right))) {
            return fail(table, "signed relational comparison is not supported", NULL);
        }
        if (binary->op == AST_BINARY_ASSIGN) {
            const ScalarSymbol *symbol = symtab_identifier(table, binary->left);
            if (symbol == NULL ||
                (symbol->declaration->type->qualifiers & AST_QUALIFIER_CONST) != 0) {
                return fail(table, "assignment requires a modifiable scalar lvalue", NULL);
            }
        }
        return 0;
    }
    return fail(table, "unsupported scalar expression", NULL);
}

static int expression_unsigned_word(const ScalarSymbols *table,
                                    const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_IDENT_EXPR) {
        const ScalarSymbol *symbol = symtab_identifier(table, expression);
        return symbol != NULL && symbol->location.width == 2 &&
               symbol->location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        return constant->type != NULL && constant->type->kind == AST_TYPE_BASE &&
               constant->type->as.base.base == AST_TYPE_INT &&
               constant->type->as.base.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op == AST_BINARY_ASSIGN) {
            return expression_unsigned_word(table, binary->left);
        }
        return expression_unsigned_word(table, binary->left) ||
               expression_unsigned_word(table, binary->right);
    }
    return 0;
}

static int expression_nonnegative(const ScalarSymbols *table,
                                 const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        return constant->constant_kind == AST_CONSTANT_CHARACTER ||
               (constant->constant_kind == AST_CONSTANT_INTEGER &&
                constant->value.integer >= 0);
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR) {
        const ScalarSymbol *symbol = symtab_identifier(table, expression);
        return symbol != NULL && symbol->location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op == AST_BINARY_ASSIGN) {
            return expression_nonnegative(table, binary->left) &&
                   expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_ADD) {
            return expression_nonnegative(table, binary->left) &&
                   expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_SUB) {
            unsigned long left, right;
            return symtab_constant(binary->left, &left) == 0 &&
                   symtab_constant(binary->right, &right) == 0 &&
                   left >= right;
        }
        return binary->op == AST_BINARY_EQUAL ||
               binary->op == AST_BINARY_NOT_EQUAL ||
               binary->op == AST_BINARY_LESS ||
               binary->op == AST_BINARY_LESS_EQUAL ||
               binary->op == AST_BINARY_GREATER ||
               binary->op == AST_BINARY_GREATER_EQUAL;
    }
    return 0;
}

static int resolve_local_declaration(ScalarSymbols *table,
                                     const AstVarDecl *variable)
{
    if (variable->declaration.node.kind != AST_NODE_VARIABLE_DECL ||
        add_variable(table, variable) != 0) {
        return -1;
    }
    if (variable->initializer != NULL &&
        (variable->initializer->kind != AST_INITIALIZER_EXPR ||
         resolve_expression(table, variable->initializer->as.expression) != 0)) {
        return fail(table, "unsupported scalar initializer", variable->name);
    }
    return 0;
}

static int resolve_statement(ScalarSymbols *table, const AstStmt *statement)
{
    if (table->returned) {
        return fail(table, "statements after return are not supported yet", NULL);
    }
    switch (statement->node.kind) {
    case AST_NODE_COMPOUND_STMT: {
        const AstCompoundStmt *block = (const AstCompoundStmt *)statement;
        size_t index;
        ++table->scope;
        for (index = 0; index < block->item_count; ++index) {
            const AstBlockItem *item = &block->items[index];
            if (item->kind == AST_BLOCK_DECLARATION) {
                const AstVarDecl *variable = (const AstVarDecl *)item->as.declaration;
                if (table->returned) {
                    return fail(table, "unsupported block declaration", NULL);
                }
                if (resolve_local_declaration(table, variable) != 0) {
                    return -1;
                }
            } else if (resolve_statement(table, item->as.statement) != 0) {
                return -1;
            }
        }
        for (index = 0; index < table->count; ++index) {
            if (table->symbols[index].scope == table->scope) {
                table->symbols[index].active = 0;
            }
        }
        --table->scope;
        return 0;
    }
    case AST_NODE_EMPTY_STMT:
        return 0;
    case AST_NODE_EXPR_STMT:
        return resolve_expression(table, ((const AstExprStmt *)statement)->expression);
    case AST_NODE_IF_STMT: {
        const AstIfStmt *conditional = (const AstIfStmt *)statement;
        int then_returns, else_returns;
        if (resolve_expression(table, conditional->condition) != 0 ||
            resolve_statement(table, conditional->then_branch) != 0) {
            return -1;
        }
        then_returns = table->returned;
        if (conditional->else_branch == NULL) {
            table->returned = 0;
            return 0;
        }
        table->returned = 0;
        if (resolve_statement(table, conditional->else_branch) != 0) {
            return -1;
        }
        else_returns = table->returned;
        table->returned = then_returns && else_returns;
        return 0;
    }
    case AST_NODE_WHILE_STMT: {
        const AstWhileStmt *loop = (const AstWhileStmt *)statement;
        int was_returned = table->returned;
        if (resolve_expression(table, loop->condition) != 0) {
            return -1;
        }
        ++table->loop_depth;
        if (resolve_statement(table, loop->body) != 0) {
            --table->loop_depth;
            return -1;
        }
        --table->loop_depth;
        table->returned = was_returned;
        return 0;
    }
    case AST_NODE_DO_WHILE_STMT: {
        const AstDoWhileStmt *loop = (const AstDoWhileStmt *)statement;
        int was_returned = table->returned;
        ++table->loop_depth;
        if (resolve_statement(table, loop->body) != 0) {
            --table->loop_depth;
            return -1;
        }
        --table->loop_depth;
        table->returned = was_returned;
        return resolve_expression(table, loop->condition);
    }
    case AST_NODE_FOR_STMT: {
        const AstForStmt *loop = (const AstForStmt *)statement;
        int was_returned = table->returned;
        ++table->scope;
        if (loop->init_declaration != NULL &&
            resolve_local_declaration(table,
                (const AstVarDecl *)loop->init_declaration) != 0) {
            --table->scope;
            return -1;
        }
        if ((loop->init_expression != NULL &&
             resolve_expression(table, loop->init_expression) != 0) ||
            (loop->condition != NULL &&
             resolve_expression(table, loop->condition) != 0)) {
            --table->scope;
            return -1;
        }
        ++table->loop_depth;
        if (resolve_statement(table, loop->body) != 0) {
            --table->loop_depth;
            --table->scope;
            return -1;
        }
        table->returned = was_returned;
        if (loop->step != NULL && resolve_expression(table, loop->step) != 0) {
            --table->loop_depth;
            --table->scope;
            return -1;
        }
        --table->loop_depth;
        {
            size_t index;
            for (index = 0; index < table->count; ++index) {
                if (table->symbols[index].scope == table->scope) {
                    table->symbols[index].active = 0;
                }
            }
        }
        --table->scope;
        return 0;
    }
    case AST_NODE_BREAK_STMT:
    case AST_NODE_CONTINUE_STMT:
        if (table->loop_depth == 0) {
            return fail(table, "break or continue used outside a loop", NULL);
        }
        return 0;
    case AST_NODE_RETURN_STMT: {
        const AstExpr *value = ((const AstReturnStmt *)statement)->expression;
        size_t width = symtab_type_width(table->function->return_type);
        if ((width == 0 && value != NULL) || (width != 0 && value == NULL)) {
            return fail(table, "return expression does not match function type", NULL);
        }
        table->returned = 1;
        return value == NULL ? 0 : resolve_expression(table, value);
    }
    default:
        return fail(table, "unsupported scalar statement", NULL);
    }
}

int symtab_resolve(ScalarSymbols *table, AstTranslationUnit *unit)
{
    size_t index;
    if (table == NULL || unit == NULL) {
        return -1;
    }
    if (table->resolved) {
        return fail(table, "symbol table can resolve only one translation unit", NULL);
    }
    table->resolved = 1;
    for (index = 0; index < unit->declarations.count; ++index) {
        const AstDecl *declaration = unit->declarations.items[index];
        size_t previous;
        const char *name;
        if (declaration->node.kind == AST_NODE_VARIABLE_DECL) {
            name = ((const AstVarDecl *)declaration)->name;
        } else if (declaration->node.kind == AST_NODE_FUNCTION_DECL) {
            name = ((const AstFunctionDecl *)declaration)->name;
        } else {
            return fail(table, "unsupported translation unit construct", NULL);
        }
        for (previous = 0; previous < index; ++previous) {
            const AstDecl *other = unit->declarations.items[previous];
            const char *other_name = other->node.kind == AST_NODE_VARIABLE_DECL ?
                ((const AstVarDecl *)other)->name : ((const AstFunctionDecl *)other)->name;
            if (strcmp(name, other_name) == 0) {
                if (declaration->node.kind != AST_NODE_FUNCTION_DECL ||
                    other->node.kind != AST_NODE_FUNCTION_DECL) {
                    return fail(table, "duplicate ordinary identifier", name);
                }
                {
                    const AstFunctionDecl *left = (const AstFunctionDecl *)declaration;
                    const AstFunctionDecl *right = (const AstFunctionDecl *)other;
                    if ((!left->is_prototype && !right->is_prototype) ||
                        left->parameter_count != right->parameter_count ||
                        left->return_type->kind != AST_TYPE_BASE ||
                        right->return_type->kind != AST_TYPE_BASE ||
                        left->return_type->as.base.base != right->return_type->as.base.base ||
                        left->return_type->as.base.is_unsigned != right->return_type->as.base.is_unsigned) {
                        return fail(table, "conflicting function declarations", name);
                    }
                }
            }
        }
    }
    for (index = 0; index < unit->declarations.count; ++index) {
        const AstDecl *declaration = unit->declarations.items[index];
        const AstFunctionDecl *function;
        size_t first_local, local;
        if (declaration->node.kind == AST_NODE_VARIABLE_DECL) {
            if (add_variable(table, (const AstVarDecl *)declaration) != 0) {
                return -1;
            }
            continue;
        }
        function = (const AstFunctionDecl *)declaration;
        if (function->parameter_count != 0 || (!function->is_prototype && function->body == NULL) ||
            function->storage != AST_STORAGE_NONE ||
            (symtab_type_width(function->return_type) == 0 &&
             (function->return_type->kind != AST_TYPE_BASE ||
              function->return_type->as.base.base != AST_TYPE_VOID))) {
            return fail(table, "unsupported translation unit construct: function signature", function->name);
        }
        if (function->is_prototype) {
            continue;
        }
        table->function = function;
        table->local_bytes = 0;
        table->returned = 0;
        first_local = table->count;
        if (resolve_statement(table, (const AstStmt *)function->body) != 0) {
            return -1;
        }
        if (symtab_type_width(function->return_type) != 0 && !table->returned) {
            return fail(table, "non-void function requires a return", function->name);
        }
        for (local = first_local; local < table->count; ++local) {
            table->symbols[local].location.address += (int)table->local_bytes;
        }
        table->function = NULL;
    }
    return 0;
}

const ScalarSymbol *symtab_identifier(const ScalarSymbols *table, const AstExpr *expression)
{
    size_t index;
    if (table == NULL) {
        return NULL;
    }
    for (index = 0; index < table->binding_count; ++index) {
        if (table->bindings[index].expression == expression) {
            return &table->symbols[table->bindings[index].symbol];
        }
    }
    return NULL;
}

const ScalarSymbol *symtab_declaration(const ScalarSymbols *table, const AstVarDecl *declaration)
{
    size_t index;
    for (index = 0; index < table->count; ++index) {
        if (table->symbols[index].declaration == declaration) {
            return &table->symbols[index];
        }
    }
    return NULL;
}

size_t symtab_frame_size(const ScalarSymbols *table, const AstFunctionDecl *function)
{
    size_t index, size = 0;
    for (index = 0; index < table->count; ++index) {
        if (table->symbols[index].function == function) {
            size += table->symbols[index].location.width;
        }
    }
    return size;
}
