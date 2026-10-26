#include "symtab.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Binding {
    const AstExpr *expression;
    size_t symbol;
} Binding;

typedef struct AddressBinding {
    const AstAddressExpr *expression;
    ResolvedAddress address;
} AddressBinding;

typedef struct DereferenceBinding {
    const AstDereferenceExpr *expression;
    ResolvedDereference dereference;
} DereferenceBinding;

struct ScalarSymbols {
    ScalarSymbol *symbols;
    size_t count;
    Binding *bindings;
    size_t binding_count;
    AddressBinding *addresses;
    size_t address_count;
    DereferenceBinding *dereferences;
    size_t dereference_count;
    const AstFunctionDecl *function;
    size_t scope;
    size_t local_bytes;
    size_t parameter_bytes;
    size_t loop_depth;
    AstTranslationUnit *unit;
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

static const char *symbol_name(const ScalarSymbol *symbol)
{
    return symbol->declaration != NULL ? symbol->declaration->name :
           symbol->parameter != NULL ? symbol->parameter->name : NULL;
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
        free(table->addresses);
        free(table->dereferences);
        free(table);
    }
}

const char *symtab_error(const ScalarSymbols *table)
{
    return table == NULL ? "out of memory creating symbol table" : table->error;
}

size_t symtab_type_width(const AstType *type)
{
    if (type == NULL || (type->qualifiers & ~AST_QUALIFIER_CONST) != 0) {
        return 0;
    }

    if (type->kind == AST_TYPE_POINTER) {
        const AstType *target = type->as.pointer.target;
        return target != NULL && target->kind == AST_TYPE_BASE &&
               (target->as.base.base == AST_TYPE_CHAR ||
                target->as.base.base == AST_TYPE_INT) &&
               (target->qualifiers &
                ~(AST_QUALIFIER_CONST | AST_QUALIFIER_VOLATILE)) == 0
            ? 2 : 0;
    }
    if (type->kind != AST_TYPE_BASE) {
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

int symtab_initializer_constant(const AstExpr *expression,
                                unsigned long *value)
{
    if (expression == NULL || value == NULL) {
        return -1;
    }
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        if (constant->constant_kind == AST_CONSTANT_INTEGER &&
            constant->value.integer >= 0 && constant->value.integer <= 65535) {
            *value = (unsigned long)constant->value.integer;
            return 0;
        }
        if (constant->constant_kind == AST_CONSTANT_CHARACTER &&
            constant->value.character <= 255) {
            *value = constant->value.character;
            return 0;
        }
        return -1;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        unsigned long left, right;
        if (binary->op != AST_BINARY_ADD && binary->op != AST_BINARY_SUB) {
            return -1;
        }
        if (symtab_initializer_constant(binary->left, &left) != 0 ||
            symtab_initializer_constant(binary->right, &right) != 0) {
            return -1;
        }
        *value = binary->op == AST_BINARY_ADD
            ? (left + right) & 65535UL
            : (left - right) & 65535UL;
        return 0;
    }
    return -1;
}

static int array_layout(ScalarSymbols *table, AstType *type,
                        const AstInitializer *initializer,
                        size_t *element_width, size_t *length)
{
    unsigned long extent;

    if (type == NULL || type->kind != AST_TYPE_ARRAY ||
        type->as.array.element == NULL ||
        type->as.array.element->kind != AST_TYPE_BASE ||
        (*element_width = symtab_type_width(type->as.array.element)) == 0) {
        return fail(table, "only byte and word arrays are supported", NULL);
    }
    if (type->as.array.length == NULL) {
        if (initializer == NULL || initializer->kind != AST_INITIALIZER_LIST ||
            initializer->as.list.count == 0) {
            return fail(table, "array requires a constant extent or nonempty initializer", NULL);
        }
        *length = initializer->as.list.count;
    } else {
        if (symtab_constant(type->as.array.length, &extent) != 0 || extent == 0 ||
            extent > 65535) {
            return fail(table, "array extent must be a positive constant", NULL);
        }
        *length = (size_t)extent;
    }
    if (*length > (size_t)-1 / *element_width) {
        return fail(table, "array size exceeds supported range", NULL);
    }
    type->as.array.resolved_length = *length;
    type->as.array.has_resolved_length = 1;
    if (initializer != NULL &&
        (initializer->kind != AST_INITIALIZER_LIST ||
         initializer->as.list.count > *length)) {
        return fail(table, "array initializer must be a fitting brace list", NULL);
    }
    return 0;
}

static int add_variable(ScalarSymbols *table, const AstVarDecl *variable)
{
    ScalarSymbol symbol;
    ScalarSymbol *grown;
    size_t index, allocation_width;
    unsigned long address, initial_value;

    memset(&symbol, 0, sizeof(symbol));
    if (variable->name == NULL || variable->storage != AST_STORAGE_NONE) {
        return fail(table, "unsupported scalar declaration", variable->name);
    }
    if (variable->type != NULL && variable->type->kind == AST_TYPE_ARRAY) {
        size_t element_width;
        if (array_layout(table, variable->type, variable->initializer,
                         &element_width, &symbol.array_length) != 0) {
            return -1;
        }
        symbol.is_array = 1;
        symbol.location.width = element_width;
        allocation_width = element_width * symbol.array_length;
    } else {
        symbol.location.width = symtab_type_width(variable->type);
        allocation_width = symbol.location.width;
    }
    if (symbol.location.width == 0) {
        return fail(table, "unsupported scalar declaration", variable->name);
    }
    symbol.allocation_width = allocation_width;
    for (index = 0; index < table->count; ++index) {
        const ScalarSymbol *other = &table->symbols[index];
        if (other->active && other->scope == table->scope &&
            strcmp(symbol_name(other), variable->name) == 0) {
            return fail(table, "duplicate scalar declaration", variable->name);
        }
    }
    symbol.declaration = variable;
    symbol.function = table->function;
    symbol.scope = table->scope;
    symbol.active = 1;
    symbol.location.is_unsigned = symbol.is_array
        ? variable->type->as.array.element->as.base.is_unsigned
        : variable->type->kind == AST_TYPE_BASE
            ? variable->type->as.base.is_unsigned : 1;
    if (table->function != NULL) {
        if (variable->attributes.has_address || variable->attributes.is_xram) {
            return fail(table, "local storage attributes are not supported yet", variable->name);
        }
        if (allocation_width > (size_t)-1 - table->local_bytes) {
            return fail(table, "local array size exceeds supported range", variable->name);
        }
        table->local_bytes += allocation_width;
        if (table->local_bytes > 32) {
            return fail(table, "leaf scalar frame exceeds 32 bytes", variable->name);
        }
        symbol.location.storage = SCALAR_LOCAL;
        symbol.location.address = -(int)table->local_bytes -
                                  (symbol.location.width == 1 ? 2 : 1);
    } else {
        if (!symbol.is_array && variable->initializer != NULL &&
            (variable->initializer->kind != AST_INITIALIZER_EXPR ||
             symtab_initializer_constant(variable->initializer->as.expression,
                                         &initial_value) != 0)) {
            return fail(table, "global initializer must be a scalar constant expression",
                        variable->name);
        }
        if (!symbol.is_array && variable->type->kind == AST_TYPE_POINTER &&
            variable->initializer != NULL &&
            (symtab_initializer_constant(variable->initializer->as.expression,
                                         &initial_value) != 0 ||
             initial_value != 0)) {
            return fail(table, "global pointer initializer must be a null pointer constant",
                        variable->name);
        }
        if (symbol.is_array && variable->initializer != NULL) {
            for (index = 0; index < variable->initializer->as.list.count; ++index) {
                const AstInitializer *item = variable->initializer->as.list.items[index];
                if (item->kind != AST_INITIALIZER_EXPR ||
                    symtab_initializer_constant(item->as.expression,
                                                &initial_value) != 0) {
                    return fail(table, "global array initializer must contain scalar constants",
                                variable->name);
                }
            }
        }
        if (!variable->attributes.has_address ||
            symtab_constant(variable->attributes.address, &address) != 0) {
            return fail(table, "global scalars require a constant __at address", variable->name);
        }
        symbol.location.storage = variable->attributes.is_xram ? SCALAR_XRAM : SCALAR_REGISTER;
        if (symbol.location.storage == SCALAR_REGISTER &&
            (address < 8 || address + allocation_width > 96)) {
            return fail(table, "register scalar must fit in internal RAM [8, 96)", variable->name);
        }
        if (address + allocation_width > 65536) {
            return fail(table, "scalar address exceeds target memory", variable->name);
        }
        symbol.location.address = (int)address;
        for (index = 0; index < table->count; ++index) {
            const ScalarLocation *other = &table->symbols[index].location;
            if (other->storage == symbol.location.storage &&
                (int)address < other->address + (int)table->symbols[index].allocation_width &&
                other->address < (int)address + (int)allocation_width) {
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

static int add_parameter(ScalarSymbols *table, const AstParamDecl *parameter)
{
    ScalarSymbol symbol;
    ScalarSymbol *grown;
    size_t width = symtab_type_width(parameter->type);
    size_t index;

    if (width == 0 || parameter->name == NULL) {
        return fail(table, "function definition requires named scalar parameters",
                    parameter->name);
    }
    for (index = 0; index < table->count; ++index) {
        const ScalarSymbol *other = &table->symbols[index];
        if (other->active && other->scope == table->scope &&
            strcmp(symbol_name(other), parameter->name) == 0) {
            return fail(table, "duplicate parameter name", parameter->name);
        }
    }
    memset(&symbol, 0, sizeof(symbol));
    symbol.parameter = parameter;
    symbol.function = table->function;
    symbol.scope = table->scope;
    symbol.active = 1;
    symbol.location.storage = SCALAR_LOCAL;
    symbol.location.width = width;
    symbol.location.is_unsigned = parameter->type->kind == AST_TYPE_BASE
        ? parameter->type->as.base.is_unsigned : 1;
    table->parameter_bytes += width;
    grown = (ScalarSymbol *)realloc(table->symbols,
        (table->count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return fail(table, "out of memory adding parameter", parameter->name);
    }
    table->symbols = grown;
    table->symbols[table->count++] = symbol;
    return 0;
}

static int expression_unsigned_word(const ScalarSymbols *table,
                                    const AstExpr *expression);
static int expression_nonnegative(const ScalarSymbols *table,
                                  const AstExpr *expression);

static int expression_contains_void_call(const ScalarSymbols *table,
                                         const AstExpr *expression)
{
    if (expression == NULL) {
        return 0;
    }
    if (expression->node.kind == AST_NODE_CALL_EXPR) {
        const AstCallExpr *call = (const AstCallExpr *)expression;
        size_t index;
        if (call->function->node.kind == AST_NODE_IDENT_EXPR) {
            const AstFunctionDecl *function = symtab_function(table,
                ((const AstIdentExpr *)call->function)->name);
            if (function != NULL && symtab_type_width(function->return_type) == 0) {
                return 1;
            }
        }
        for (index = 0; index < call->arguments.count; ++index) {
            if (expression_contains_void_call(table, call->arguments.items[index])) {
                return 1;
            }
        }
    } else if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        return expression_contains_void_call(table, binary->left) ||
               expression_contains_void_call(table, binary->right);
    } else if (expression->node.kind == AST_NODE_INDEX_EXPR) {
        return expression_contains_void_call(table,
            ((const AstIndexExpr *)expression)->index);
    }
    return 0;
}

static int bind_identifier(ScalarSymbols *table, const AstExpr *expression,
                           const char *name, size_t *symbol_index)
{
    size_t index = table->count;
    while (index != 0) {
        const ScalarSymbol *symbol = &table->symbols[--index];
        if (symbol->active && strcmp(symbol_name(symbol), name) == 0) {
            Binding *grown = realloc(table->bindings,
                (table->binding_count + 1) * sizeof(*grown));
            if (grown == NULL) {
                return fail(table, "out of memory binding scalar", name);
            }
            table->bindings = grown;
            grown[table->binding_count].expression = expression;
            grown[table->binding_count++].symbol = index;
            *symbol_index = index;
            return 0;
        }
    }
    return fail(table, "undeclared scalar", name);
}

static int bind_address(ScalarSymbols *table,
                        const AstAddressExpr *expression,
                        const AstType *pointee_type,
                        ScalarLocation location)
{
    AddressBinding *grown;
    ResolvedAddress address;

    if (pointee_type == NULL || pointee_type->kind != AST_TYPE_BASE ||
        location.storage == SCALAR_LOCAL ||
        (location.storage != SCALAR_REGISTER &&
         location.storage != SCALAR_XRAM) ||
        symtab_type_width(pointee_type) == 0 ||
        location.width != symtab_type_width(pointee_type)) {
        return fail(table, "address-of requires a byte or word register/XRAM object",
                    NULL);
    }
    if (location.storage == SCALAR_XRAM && location.address < 256) {
        return fail(table,
                    "XRAM pointers require addresses at or above 256 to distinguish them from register addresses",
                    NULL);
    }
    address.pointee_type = pointee_type;
    address.location = location;
    address.pointer_width = 2;
    grown = realloc(table->addresses,
                    (table->address_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return fail(table, "out of memory resolving object address", NULL);
    }
    table->addresses = grown;
    grown[table->address_count].expression = expression;
    grown[table->address_count++].address = address;
    return 0;
}

static int bind_dereference(ScalarSymbols *table,
                            const AstDereferenceExpr *expression,
                            const AstType *pointee_type)
{
    DereferenceBinding *grown;
    ResolvedDereference dereference;

    dereference.width = symtab_type_width(pointee_type);
    if (pointee_type == NULL || pointee_type->kind != AST_TYPE_BASE ||
        dereference.width == 0) {
        return fail(table, "dereference requires a pointer to byte or word", NULL);
    }
    dereference.pointee_type = pointee_type;
    dereference.is_unsigned = pointee_type->as.base.is_unsigned;
    grown = realloc(table->dereferences,
                    (table->dereference_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return fail(table, "out of memory resolving pointer dereference", NULL);
    }
    table->dereferences = grown;
    grown[table->dereference_count].expression = expression;
    grown[table->dereference_count++].dereference = dereference;
    return 0;
}

static const AstType *expression_pointer_pointee(const ScalarSymbols *table,
                                                 const AstExpr *expression)
{
    if (expression == NULL) {
        return NULL;
    }
    if (expression->node.kind == AST_NODE_ADDRESS_EXPR) {
        ResolvedAddress address;
        return symtab_address_info(table, (const AstAddressExpr *)expression,
                                   &address) == 0
            ? address.pointee_type : NULL;
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR) {
        const ScalarSymbol *symbol = symtab_identifier(table, expression);
        const AstType *type = symbol == NULL ? NULL :
            symbol->declaration != NULL ? symbol->declaration->type :
            symbol->parameter->type;
        return type != NULL && type->kind == AST_TYPE_POINTER
            ? type->as.pointer.target : NULL;
    }
    return NULL;
}

static int expression_is_pointer(const ScalarSymbols *table,
                                 const AstExpr *expression)
{
    return expression_pointer_pointee(table, expression) != NULL;
}

static int compatible_pointee_type(const AstType *left, const AstType *right)
{
    return left != NULL && right != NULL &&
           left->kind == AST_TYPE_BASE && right->kind == AST_TYPE_BASE &&
           left->as.base.base == right->as.base.base &&
           left->as.base.is_unsigned == right->as.base.is_unsigned &&
           left->qualifiers == right->qualifiers;
}

static int compatible_pointer_value(const ScalarSymbols *table,
                                    const AstType *target_type,
                                    const AstExpr *value)
{
    const AstType *pointee = expression_pointer_pointee(table, value);
    return target_type != NULL && target_type->kind == AST_TYPE_POINTER &&
           compatible_pointee_type(target_type->as.pointer.target, pointee);
}

static int is_null_pointer_constant(const AstExpr *expression)
{
    unsigned long value;
    return symtab_initializer_constant(expression, &value) == 0 && value == 0;
}

static const AstType *expression_pointer_pointee(const ScalarSymbols *table,
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
        size_t symbol_index;
        if (bind_identifier(table, expression, name, &symbol_index) != 0) {
            return -1;
        }
        return table->symbols[symbol_index].is_array
            ? fail(table, "array value requires an index", name) : 0;
    }
    if (expression->node.kind == AST_NODE_ADDRESS_EXPR) {
        const AstAddressExpr *address = (const AstAddressExpr *)expression;
        const AstType *pointee_type;
        ScalarLocation location;
        size_t symbol_index;
        if (address->operand == NULL) {
            return fail(table, "address-of requires a supported lvalue", NULL);
        }
        if (address->operand->node.kind == AST_NODE_IDENT_EXPR) {
            const AstIdentExpr *identifier =
                (const AstIdentExpr *)address->operand;
            const ScalarSymbol *symbol;
            if (symtab_function(table, identifier->name) != NULL) {
                return fail(table, "function pointers are not supported",
                            identifier->name);
            }
            if (bind_identifier(table, address->operand, identifier->name,
                                &symbol_index) != 0) {
                return -1;
            }
            symbol = &table->symbols[symbol_index];
            if (symbol->is_array) {
                return fail(table, "whole-array addresses are not supported",
                            identifier->name);
            }
            pointee_type = symbol->declaration != NULL
                ? symbol->declaration->type : symbol->parameter->type;
            location = symbol->location;
        } else if (address->operand->node.kind == AST_NODE_INDEX_EXPR) {
            const AstIndexExpr *index =
                (const AstIndexExpr *)address->operand;
            const AstIdentExpr *identifier;
            const ScalarSymbol *symbol;
            if (index->array == NULL ||
                index->array->node.kind != AST_NODE_IDENT_EXPR) {
                return fail(table, "address-of array element requires a declared array",
                            NULL);
            }
            identifier = (const AstIdentExpr *)index->array;
            if (bind_identifier(table, index->array, identifier->name,
                                &symbol_index) != 0) {
                return -1;
            }
            symbol = &table->symbols[symbol_index];
            if (!symbol->is_array ||
                symtab_index_location(table, index, &location) != 0) {
                return fail(table,
                            "address-of array element requires an in-bounds constant index",
                            identifier->name);
            }
            pointee_type = symbol->declaration->type->as.array.element;
        } else {
            return fail(table, "address-of requires a supported scalar lvalue", NULL);
        }
        return bind_address(table, address, pointee_type, location);
    }
    if (expression->node.kind == AST_NODE_DEREFERENCE_EXPR) {
        const AstDereferenceExpr *dereference =
            (const AstDereferenceExpr *)expression;
        const AstType *pointee_type;
        if (dereference->operand == NULL ||
            resolve_expression(table, dereference->operand) != 0) {
            return fail(table, "dereference requires a resolved pointer value", NULL);
        }
        pointee_type = expression_pointer_pointee(table, dereference->operand);
        if (pointee_type == NULL) {
            return fail(table, "dereference requires a pointer to byte or word", NULL);
        }
        return bind_dereference(table, dereference, pointee_type);
    }
    if (expression->node.kind == AST_NODE_INDEX_EXPR) {
        const AstIndexExpr *index = (const AstIndexExpr *)expression;
        const AstIdentExpr *array;
        const ScalarSymbol *symbol;
        unsigned long element;
        size_t symbol_index;
        if (index->array == NULL ||
            index->array->node.kind != AST_NODE_IDENT_EXPR) {
            return fail(table, "array base must be a declared array", NULL);
        }
        array = (const AstIdentExpr *)index->array;
        if (bind_identifier(table, index->array, array->name, &symbol_index) != 0) {
            return -1;
        }
        symbol = &table->symbols[symbol_index];
        if (!symbol->is_array) {
            return fail(table, "subscripted object is not an array", array->name);
        }
        if (symtab_initializer_constant(index->index, &element) == 0) {
            if (element >= symbol->array_length) {
                return fail(table, "constant array index is out of bounds", array->name);
            }
            return 0;
        }
        if (expression_contains_void_call(table, index->index) ||
            resolve_expression(table, index->index) != 0 ||
            expression_is_pointer(table, index->index)) {
            return fail(table, "array index requires an integer value", array->name);
        }
        return 0;
    }
    if (expression->node.kind == AST_NODE_CALL_EXPR) {
        const AstCallExpr *call = (const AstCallExpr *)expression;
        const AstIdentExpr *identifier;
        const AstFunctionDecl *function;
        size_t index;
        if (call->function == NULL ||
            call->function->node.kind != AST_NODE_IDENT_EXPR) {
            return fail(table, "only direct function calls are supported", NULL);
        }
        identifier = (const AstIdentExpr *)call->function;
        function = symtab_function(table, identifier->name);
        if (function == NULL) {
            return fail(table, "call to undeclared function", identifier->name);
        }
        if (call->arguments.count != function->parameter_count) {
            return fail(table, "function call has the wrong number of arguments",
                        function->name);
        }
        for (index = 0; index < call->arguments.count; ++index) {
            const AstType *parameter_type = function->parameters[index]->type;
            const AstExpr *argument = call->arguments.items[index];
            if (symtab_type_width(parameter_type) == 0 ||
                expression_contains_void_call(table, argument) ||
                resolve_expression(table, argument) != 0) {
                return fail(table, "unsupported function argument", function->name);
            }
            if (parameter_type->kind == AST_TYPE_POINTER) {
                if (!compatible_pointer_value(table, parameter_type, argument) &&
                    !is_null_pointer_constant(argument)) {
                    return fail(table, "function argument has incompatible pointer type",
                                function->name);
                }
            } else if (expression_is_pointer(table, argument)) {
                return fail(table, "pointer argument requires a pointer parameter",
                            function->name);
            }
        }
        return 0;
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
        if (expression_contains_void_call(table, binary->left) ||
            expression_contains_void_call(table, binary->right)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (resolve_expression(table, binary->left) != 0 ||
            resolve_expression(table, binary->right) != 0) {
            return -1;
        }
        if (binary->op == AST_BINARY_ASSIGN) {
            const ScalarSymbol *symbol = NULL;
            const AstType *target_type = NULL;
            int modifiable;

            if (binary->left->node.kind == AST_NODE_IDENT_EXPR) {
                symbol = symtab_identifier(table, binary->left);
            } else if (binary->left->node.kind == AST_NODE_INDEX_EXPR) {
                symbol = symtab_identifier(table,
                    ((const AstIndexExpr *)binary->left)->array);
            }
            if (symbol != NULL) {
                target_type = symbol->is_array
                    ? symbol->declaration->type->as.array.element
                    : symbol->declaration != NULL
                        ? symbol->declaration->type : symbol->parameter->type;
            } else if (binary->left->node.kind == AST_NODE_DEREFERENCE_EXPR) {
                ResolvedDereference resolved;
                if (symtab_dereference_info(table,
                        (const AstDereferenceExpr *)binary->left,
                        &resolved) == 0) {
                    target_type = resolved.pointee_type;
                }
            }
            modifiable = target_type != NULL &&
                (target_type->qualifiers & AST_QUALIFIER_CONST) == 0 &&
                (binary->left->node.kind == AST_NODE_IDENT_EXPR ||
                 binary->left->node.kind == AST_NODE_INDEX_EXPR ||
                 binary->left->node.kind == AST_NODE_DEREFERENCE_EXPR);

            if (!modifiable) {
                return fail(table, "assignment requires a modifiable scalar lvalue", NULL);
            }
            if (target_type->kind == AST_TYPE_POINTER) {
                if (binary->left->node.kind != AST_NODE_IDENT_EXPR ||
                    (!compatible_pointer_value(table, target_type, binary->right) &&
                     !is_null_pointer_constant(binary->right))) {
                    return fail(table, "assignment has incompatible pointer type", NULL);
                }
            } else if (expression_is_pointer(table, binary->right)) {
                return fail(table, "pointer value cannot be assigned to a scalar", NULL);
            }
            return 0;
        }
        if (expression_is_pointer(table, binary->left) ||
            expression_is_pointer(table, binary->right)) {
            return fail(table, "pointer arithmetic and other pointer operators are not supported",
                        NULL);
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
        return 0;
    }
    return fail(table, "unsupported scalar expression", NULL);
}

static int expression_unsigned_word(const ScalarSymbols *table,
                                    const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_DEREFERENCE_EXPR) {
        ResolvedDereference dereference;
        return symtab_dereference_info(table,
            (const AstDereferenceExpr *)expression, &dereference) == 0 &&
            dereference.width == 2 && dereference.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR ||
        expression->node.kind == AST_NODE_INDEX_EXPR) {
        const AstExpr *identifier = expression->node.kind == AST_NODE_INDEX_EXPR
            ? ((const AstIndexExpr *)expression)->array : expression;
        const ScalarSymbol *symbol = symtab_identifier(table, identifier);
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
    if (expression->node.kind == AST_NODE_IDENT_EXPR ||
        expression->node.kind == AST_NODE_INDEX_EXPR) {
        const AstExpr *identifier = expression->node.kind == AST_NODE_INDEX_EXPR
            ? ((const AstIndexExpr *)expression)->array : expression;
        const ScalarSymbol *symbol = symtab_identifier(table, identifier);
        return symbol != NULL && symbol->location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_DEREFERENCE_EXPR) {
        ResolvedDereference dereference;
        return symtab_dereference_info(table,
            (const AstDereferenceExpr *)expression, &dereference) == 0 &&
            dereference.is_unsigned;
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
    size_t index;
    if (variable->declaration.node.kind != AST_NODE_VARIABLE_DECL ||
        add_variable(table, variable) != 0) {
        return -1;
    }
    if (variable->initializer != NULL) {
        if (variable->type->kind == AST_TYPE_ARRAY) {
            for (index = 0; index < variable->initializer->as.list.count; ++index) {
                const AstInitializer *item = variable->initializer->as.list.items[index];
                if (item->kind != AST_INITIALIZER_EXPR ||
                    expression_contains_void_call(table, item->as.expression) ||
                    resolve_expression(table, item->as.expression) != 0 ||
                    expression_is_pointer(table, item->as.expression)) {
                    return fail(table, "unsupported array initializer", variable->name);
                }
            }
        } else if (variable->initializer->kind != AST_INITIALIZER_EXPR ||
                   expression_contains_void_call(table,
                       variable->initializer->as.expression) ||
                   resolve_expression(table,
                       variable->initializer->as.expression) != 0) {
            return fail(table, "unsupported scalar initializer", variable->name);
        } else if (variable->type->kind == AST_TYPE_POINTER) {
            const AstExpr *value = variable->initializer->as.expression;
            if (!compatible_pointer_value(table, variable->type, value) &&
                !is_null_pointer_constant(value)) {
                return fail(table, "initializer has incompatible pointer type",
                            variable->name);
            }
        } else if (expression_is_pointer(table,
                       variable->initializer->as.expression)) {
            return fail(table, "pointer value cannot initialize a scalar",
                        variable->name);
        }
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
        if (((const AstExprStmt *)statement)->expression != NULL &&
            ((const AstExprStmt *)statement)->expression->node.kind !=
                AST_NODE_CALL_EXPR &&
            expression_contains_void_call(table,
                ((const AstExprStmt *)statement)->expression)) {
            return fail(table, "void function call used as a value", NULL);
        }
        return resolve_expression(table, ((const AstExprStmt *)statement)->expression);
    case AST_NODE_IF_STMT: {
        const AstIfStmt *conditional = (const AstIfStmt *)statement;
        int then_returns, else_returns;
        if (expression_contains_void_call(table, conditional->condition)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (resolve_expression(table, conditional->condition) != 0) {
            return -1;
        }
        if (expression_is_pointer(table, conditional->condition)) {
            return fail(table, "pointer value is not a supported condition", NULL);
        }
        if (resolve_statement(table, conditional->then_branch) != 0) {
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
        if (expression_contains_void_call(table, loop->condition)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (resolve_expression(table, loop->condition) != 0) {
            return -1;
        }
        if (expression_is_pointer(table, loop->condition)) {
            return fail(table, "pointer value is not a supported condition", NULL);
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
        if (expression_contains_void_call(table, loop->condition)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (resolve_expression(table, loop->condition) != 0) {
            return -1;
        }
        if (expression_is_pointer(table, loop->condition)) {
            return fail(table, "pointer value is not a supported condition", NULL);
        }
        return 0;
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
             resolve_expression(table, loop->init_expression) != 0)) {
            --table->scope;
            return -1;
        }
        if (loop->condition != NULL &&
            expression_contains_void_call(table, loop->condition)) {
            --table->scope;
            return fail(table, "void function call used as a value", NULL);
        }
        if (loop->condition != NULL &&
            resolve_expression(table, loop->condition) != 0) {
            --table->scope;
            return -1;
        }
        if (expression_is_pointer(table, loop->condition)) {
            --table->scope;
            return fail(table, "pointer value is not a supported condition", NULL);
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
        if (value != NULL && expression_contains_void_call(table, value)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (value != NULL && resolve_expression(table, value) != 0) {
            return -1;
        }
        if (expression_is_pointer(table, value)) {
            return fail(table, "pointer return values are not supported yet", NULL);
        }
        table->returned = 1;
        return 0;
    }
    default:
        return fail(table, "unsupported scalar statement", NULL);
    }
}

static int compatible_type(const AstType *left, const AstType *right)
{
    if (left == NULL || right == NULL || left->kind != right->kind) {
        return 0;
    }
    if (left->kind == AST_TYPE_POINTER) {
        return compatible_pointee_type(left->as.pointer.target,
                                       right->as.pointer.target);
    }
    return left->kind == AST_TYPE_BASE &&
           left->as.base.base == right->as.base.base &&
           left->as.base.is_unsigned == right->as.base.is_unsigned;
}

static int compatible_function(const AstFunctionDecl *left,
                               const AstFunctionDecl *right)
{
    size_t index;
    if (!compatible_type(left->return_type, right->return_type) ||
        left->parameter_count != right->parameter_count) {
        return 0;
    }
    for (index = 0; index < left->parameter_count; ++index) {
        if (!compatible_type(left->parameters[index]->type,
                             right->parameters[index]->type)) {
            return 0;
        }
    }
    return 1;
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
    table->unit = unit;
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
                        !compatible_function(left, right)) {
                        return fail(table, "conflicting function declarations", name);
                    }
                }
            }
        }
    }
    for (index = 0; index < unit->declarations.count; ++index) {
        const AstDecl *declaration = unit->declarations.items[index];
        const AstFunctionDecl *function;
        size_t local;
        if (declaration->node.kind == AST_NODE_VARIABLE_DECL) {
            if (add_variable(table, (const AstVarDecl *)declaration) != 0) {
                return -1;
            }
            continue;
        }
        function = (const AstFunctionDecl *)declaration;
        if ((!function->is_prototype && function->body == NULL) ||
            function->storage != AST_STORAGE_NONE ||
            function->return_type->kind == AST_TYPE_POINTER ||
            (symtab_type_width(function->return_type) == 0 &&
             (function->return_type->kind != AST_TYPE_BASE ||
              function->return_type->as.base.base != AST_TYPE_VOID))) {
            return fail(table, "unsupported translation unit construct: function signature", function->name);
        }
        {
            size_t parameter;
            for (parameter = 0; parameter < function->parameter_count; ++parameter) {
                if (symtab_type_width(function->parameters[parameter]->type) == 0) {
                    return fail(table, "unsupported function parameter type",
                                function->name);
                }
            }
        }
        if (function->is_prototype) {
            continue;
        }
        table->function = function;
        table->scope = 0;
        table->local_bytes = 0;
        table->parameter_bytes = 0;
        table->returned = 0;
        {
            size_t parameter;
            for (parameter = 0; parameter < function->parameter_count; ++parameter) {
                table->scope = 1;
                if (add_parameter(table, function->parameters[parameter]) != 0) {
                    return -1;
                }
            }
            table->scope = 0;
        }
        if (resolve_statement(table, (const AstStmt *)function->body) != 0) {
            return -1;
        }
        if (symtab_type_width(function->return_type) != 0 && !table->returned) {
            return fail(table, "non-void function requires a return", function->name);
        }
        for (local = 0; local < table->count; ++local) {
            ScalarSymbol *symbol = &table->symbols[local];
            if (symbol->function != function) {
                continue;
            }
            if (symbol->parameter != NULL) {
                size_t parameter, offset = 1;
                for (parameter = 0; parameter < function->parameter_count; ++parameter) {
                    if (function->parameters[parameter] == symbol->parameter) {
                        symbol->location.address = (int)(table->local_bytes +
                            table->parameter_bytes - offset);
                        break;
                    }
                    offset += symtab_type_width(function->parameters[parameter]->type);
                }
            } else {
                symbol->location.address += (int)table->local_bytes;
            }
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

int symtab_index_location(const ScalarSymbols *table,
                          const AstIndexExpr *expression,
                          ScalarLocation *location)
{
    const ScalarSymbol *symbol;
    unsigned long element;
    size_t offset;

    if (table == NULL || expression == NULL || location == NULL ||
        expression->array == NULL ||
        expression->array->node.kind != AST_NODE_IDENT_EXPR ||
        symtab_constant(expression->index, &element) != 0) {
        return -1;
    }
    symbol = symtab_identifier(table, expression->array);
    if (symbol == NULL || !symbol->is_array ||
        element >= symbol->array_length ||
        element > (size_t)-1 / symbol->location.width) {
        return -1;
    }
    offset = (size_t)element * symbol->location.width;
    if (offset > (size_t)INT_MAX ||
        symbol->location.address > INT_MAX - (int)offset) {
        return -1;
    }
    *location = symbol->location;
    location->address += (int)offset;
    return 0;
}

int symtab_address_info(const ScalarSymbols *table,
                        const AstAddressExpr *expression,
                        ResolvedAddress *address)
{
    size_t index;

    if (table == NULL || expression == NULL || address == NULL) {
        return -1;
    }
    for (index = 0; index < table->address_count; ++index) {
        if (table->addresses[index].expression == expression) {
            *address = table->addresses[index].address;
            return 0;
        }
    }
    return -1;
}

int symtab_dereference_info(const ScalarSymbols *table,
                            const AstDereferenceExpr *expression,
                            ResolvedDereference *dereference)
{
    size_t index;

    if (table == NULL || expression == NULL || dereference == NULL) {
        return -1;
    }
    for (index = 0; index < table->dereference_count; ++index) {
        if (table->dereferences[index].expression == expression) {
            *dereference = table->dereferences[index].dereference;
            return 0;
        }
    }
    return -1;
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

const ScalarSymbol *symtab_parameter(const ScalarSymbols *table,
                                     const AstParamDecl *parameter)
{
    size_t index;
    if (table == NULL) {
        return NULL;
    }
    for (index = 0; index < table->count; ++index) {
        if (table->symbols[index].parameter == parameter) {
            return &table->symbols[index];
        }
    }
    return NULL;
}

const AstFunctionDecl *symtab_function(const ScalarSymbols *table,
                                      const char *name)
{
    size_t index;
    const AstFunctionDecl *prototype = NULL;
    if (table == NULL || table->unit == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < table->unit->declarations.count; ++index) {
        const AstDecl *declaration = table->unit->declarations.items[index];
        if (declaration->node.kind == AST_NODE_FUNCTION_DECL &&
            strcmp(((const AstFunctionDecl *)declaration)->name, name) == 0) {
            const AstFunctionDecl *function = (const AstFunctionDecl *)declaration;
            if (!function->is_prototype) {
                return function;
            }
            prototype = function;
        }
    }
    return prototype;
}

size_t symtab_frame_size(const ScalarSymbols *table, const AstFunctionDecl *function)
{
    size_t index, size = 0;
    for (index = 0; index < table->count; ++index) {
        if (table->symbols[index].function == function &&
            table->symbols[index].declaration != NULL) {
            size += table->symbols[index].allocation_width;
        }
    }
    return size;
}
