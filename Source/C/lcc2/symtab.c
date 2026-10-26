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

typedef struct StructureLayout {
    const AstStructDecl *declaration;
    ResolvedStructMember *members;
    size_t member_count;
    size_t size;
} StructureLayout;

typedef struct MemberBinding {
    const AstMemberExpr *expression;
    const AstFunctionDecl *function;
    ResolvedMemberAccess access;
} MemberBinding;

struct ScalarSymbols {
    ScalarSymbol *symbols;
    size_t count;
    Binding *bindings;
    size_t binding_count;
    AddressBinding *addresses;
    size_t address_count;
    DereferenceBinding *dereferences;
    size_t dereference_count;
    StructureLayout *structures;
    size_t structure_count;
    MemberBinding *member_bindings;
    size_t member_binding_count;
    const AstFunctionDecl *function;
    size_t scope;
    size_t local_bytes;
    size_t parameter_bytes;
    size_t loop_depth;
    size_t switch_depth;
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
        {
            size_t index;
            for (index = 0; index < table->structure_count; ++index) {
                free(table->structures[index].members);
            }
        }
        free(table->structures);
        free(table->member_bindings);
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
            ? 2 :
               target != NULL && target->kind == AST_TYPE_STRUCT &&
               target->as.structure.declaration != NULL &&
               target->as.structure.declaration->is_complete &&
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

static const StructureLayout *find_structure_layout(
    const ScalarSymbols *table, const AstStructDecl *structure)
{
    size_t index;
    for (index = 0; index < table->structure_count; ++index) {
        if (table->structures[index].declaration == structure) {
            return &table->structures[index];
        }
    }
    return NULL;
}

static int resolve_structure_layout(ScalarSymbols *table,
                                    const AstStructDecl *structure,
                                    size_t *size)
{
    StructureLayout layout;
    StructureLayout *grown;
    size_t member_index, field_index, field_count = 0, offset = 0;

    if (size == NULL || structure == NULL || !structure->is_complete ||
        structure->member_count == 0) {
        return fail(table,
                    "structure object requires a complete nonempty definition",
                    structure == NULL ? NULL : structure->tag);
    }
    {
        const StructureLayout *existing =
            find_structure_layout(table, structure);
        if (existing != NULL) {
            *size = existing->size;
            return 0;
        }
    }

    memset(&layout, 0, sizeof(layout));
    layout.declaration = structure;
    if (structure->members == NULL) {
        return fail(table, "invalid structure member list", structure->tag);
    }
    for (member_index = 0; member_index < structure->member_count;
         ++member_index) {
        const AstStructMember *member = &structure->members[member_index];
        if (member->declarator_count == 0 || member->declarators == NULL ||
            member->declarator_count > (size_t)-1 - field_count) {
            return fail(table, "invalid structure member declaration",
                        structure->tag);
        }
        field_count += member->declarator_count;
    }
    if (field_count == 0 ||
        field_count > (size_t)-1 / sizeof(*layout.members)) {
        return fail(table, "invalid structure member count", structure->tag);
    }
    layout.members = (ResolvedStructMember *)calloc(field_count,
                                                     sizeof(*layout.members));
    if (layout.members == NULL) {
        return fail(table, "out of memory laying out structure", structure->tag);
    }

    for (member_index = 0; member_index < structure->member_count;
         ++member_index) {
        const AstStructMember *member = &structure->members[member_index];
        for (field_index = 0; field_index < member->declarator_count;
             ++field_index) {
            const AstVarDecl *field = member->declarators[field_index];
            ResolvedStructMember *resolved;
            size_t width, previous;

            if (field == NULL || field->name == NULL || field->type == NULL ||
                field->type->kind != AST_TYPE_BASE ||
                (field->type->as.base.base != AST_TYPE_CHAR &&
                 field->type->as.base.base != AST_TYPE_INT) ||
                (width = symtab_type_width(field->type)) == 0) {
                free(layout.members);
                return fail(table,
                            "structure fields must be byte or word scalars",
                            structure->tag);
            }
            for (previous = 0; previous < layout.member_count; ++previous) {
                if (strcmp(layout.members[previous].name, field->name) == 0) {
                    free(layout.members);
                    return fail(table, "duplicate structure member", field->name);
                }
            }
            if (width > (size_t)-1 - offset || offset + width > 65536) {
                free(layout.members);
                return fail(table, "structure size exceeds target address space",
                            structure->tag);
            }
            resolved = &layout.members[layout.member_count++];
            resolved->structure = structure;
            resolved->declaration = field;
            resolved->name = field->name;
            resolved->type = field->type;
            resolved->offset = offset;
            resolved->width = width;
            offset += width;
        }
    }
    layout.size = offset;
    if (table->structure_count == (size_t)-1 ||
        table->structure_count + 1 > (size_t)-1 / sizeof(*grown)) {
        free(layout.members);
        return fail(table, "structure count exceeds supported range",
                    structure->tag);
    }
    grown = (StructureLayout *)realloc(table->structures,
        (table->structure_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        free(layout.members);
        return fail(table, "out of memory recording structure layout",
                    structure->tag);
    }
    table->structures = grown;
    table->structures[table->structure_count++] = layout;
    *size = layout.size;
    return 0;
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
        if (binary->op != AST_BINARY_ADD && binary->op != AST_BINARY_SUB &&
            binary->op != AST_BINARY_BIT_AND &&
            binary->op != AST_BINARY_BIT_OR &&
            binary->op != AST_BINARY_BIT_XOR &&
            binary->op != AST_BINARY_SHIFT_LEFT &&
            binary->op != AST_BINARY_MUL &&
            ((binary->op != AST_BINARY_DIV && binary->op != AST_BINARY_MOD &&
              binary->op != AST_BINARY_SHIFT_RIGHT) ||
             binary->left->node.kind != AST_NODE_CONSTANT_EXPR ||
             (binary->op != AST_BINARY_SHIFT_RIGHT &&
              binary->right->node.kind != AST_NODE_CONSTANT_EXPR))) {
            return -1;
        }
        if (symtab_initializer_constant(binary->left, &left) != 0 ||
            symtab_initializer_constant(binary->right, &right) != 0) {
            return -1;
        }
        if ((binary->op == AST_BINARY_DIV || binary->op == AST_BINARY_MOD) &&
            right == 0) {
            return -1;
        }
        switch (binary->op) {
        case AST_BINARY_MUL: *value = (left * right) & 65535UL; break;
        case AST_BINARY_DIV: *value = left / right; break;
        case AST_BINARY_MOD: *value = left % right; break;
        case AST_BINARY_SHIFT_LEFT:
            *value = right >= 16 ? 0 : (left << right) & 65535UL; break;
        case AST_BINARY_SHIFT_RIGHT:
            *value = right >= 16 ? 0 : left >> right; break;
        case AST_BINARY_ADD: *value = (left + right) & 65535UL; break;
        case AST_BINARY_SUB: *value = (left - right) & 65535UL; break;
        case AST_BINARY_BIT_AND: *value = left & right; break;
        case AST_BINARY_BIT_OR: *value = left | right; break;
        default: *value = left ^ right; break;
        }
        return 0;
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        unsigned long operand;
        if (unary->op != AST_UNARY_PLUS && unary->op != AST_UNARY_MINUS &&
            unary->op != AST_UNARY_BITWISE_NOT &&
            unary->op != AST_UNARY_LOGICAL_NOT) {
            return -1;
        }
        if (symtab_initializer_constant(unary->operand, &operand) != 0) {
            return -1;
        }
        switch (unary->op) {
        case AST_UNARY_PLUS: *value = operand; break;
        case AST_UNARY_MINUS: *value = (0UL - operand) & 65535UL; break;
        case AST_UNARY_BITWISE_NOT: *value = ~operand & 65535UL; break;
        default: *value = operand == 0; break;
        }
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
    } else if (variable->type != NULL &&
               variable->type->kind == AST_TYPE_STRUCT) {
        if (variable->initializer != NULL) {
            return fail(table, "aggregate initializers are not supported",
                        variable->name);
        }
        if (resolve_structure_layout(table,
                variable->type->as.structure.declaration,
                &allocation_width) != 0) {
            return -1;
        }
        symbol.is_aggregate = 1;
        symbol.location.width = allocation_width;
    } else if (variable->type != NULL &&
               variable->type->kind == AST_TYPE_UNION) {
        return fail(table, "unions are not supported", variable->name);
    } else {
        symbol.location.width = symtab_type_width(variable->type);
        allocation_width = symbol.location.width;
    }
    if (symbol.location.width == 0 || allocation_width > 65536) {
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
    symbol.location.is_unsigned = symbol.is_aggregate ? 0 : symbol.is_array
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
        {
            size_t address_width = symbol.location.width;
            if (symbol.is_aggregate) {
                const StructureLayout *layout = find_structure_layout(table,
                    variable->type->as.structure.declaration);
                address_width = layout->members[0].width;
            }
            symbol.location.address = -(int)table->local_bytes -
                                      (address_width == 1 ? 2 : 1);
        }
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
    } else if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        return expression_contains_void_call(table,
            ((const AstUnaryExpr *)expression)->operand);
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
    size_t pointee_width;

    if (pointee_type != NULL && pointee_type->kind == AST_TYPE_STRUCT) {
        if ((pointee_type->qualifiers &
             ~(AST_QUALIFIER_CONST | AST_QUALIFIER_VOLATILE)) != 0 ||
            resolve_structure_layout(table,
                pointee_type->as.structure.declaration, &pointee_width) != 0) {
            return fail(table, "address-of requires a supported complete structure",
                        NULL);
        }
    } else {
        pointee_width = symtab_type_width(pointee_type);
    }
    if (pointee_type == NULL || location.storage == SCALAR_LOCAL ||
        (location.storage != SCALAR_REGISTER &&
         location.storage != SCALAR_XRAM) ||
        pointee_width == 0 || location.width != pointee_width) {
        return fail(table, "address-of requires a supported register/XRAM object",
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
                                                 const AstExpr *expression);
static int resolve_expression(ScalarSymbols *table, const AstExpr *expression);

static int bind_member_access(ScalarSymbols *table,
                             const AstMemberExpr *expression)
{
    MemberBinding *grown;
    ResolvedMemberAccess access;
    const ScalarSymbol *symbol = NULL;
    const AstType *object_type;
    const AstStructDecl *structure;
    size_t symbol_index = 0;

    if (expression->is_arrow) {
        size_t structure_size;
        if (expression->object == NULL ||
            resolve_expression(table, expression->object) != 0) {
            return fail(table, "arrow member access requires a resolved pointer",
                        expression->field);
        }
        object_type = expression_pointer_pointee(table, expression->object);
        if (object_type == NULL || object_type->kind != AST_TYPE_STRUCT) {
            return fail(table, "arrow member access requires a pointer to structure",
                        expression->field);
        }
        structure = object_type->as.structure.declaration;
        if (resolve_structure_layout(table, structure,
                                     &structure_size) != 0) {
            return -1;
        }
        access.pointer_expression = expression->object;
        access.is_indirect = 1;
    } else if (expression->object != NULL &&
               expression->object->node.kind == AST_NODE_IDENT_EXPR) {
        const AstIdentExpr *identifier =
            (const AstIdentExpr *)expression->object;
        if (bind_identifier(table, expression->object, identifier->name,
                            &symbol_index) != 0) {
            return -1;
        }
        symbol = &table->symbols[symbol_index];
        if (symbol->declaration == NULL || symbol->is_array ||
            !symbol->is_aggregate) {
            return fail(table, "member access requires a structure object",
                        symbol_name(symbol));
        }
        object_type = symbol->declaration->type;
        if (object_type == NULL || object_type->kind != AST_TYPE_STRUCT) {
            return fail(table, "member access requires a structure object",
                        symbol_name(symbol));
        }
        structure = object_type->as.structure.declaration;
    } else {
        return fail(table, "member access requires a structure object identifier",
                    NULL);
    }
    memset(&access, 0, sizeof(access));
    if (expression->is_arrow) {
        access.pointer_expression = expression->object;
        access.is_indirect = 1;
    }
    if (symtab_structure_member(table, structure, expression->field,
                                &access.member) != 0) {
        return fail(table, "unknown structure member", expression->field);
    }
    if (access.is_indirect) {
        access.location.storage = SCALAR_REGISTER;
        access.location.width = access.member.width;
        access.location.is_unsigned =
            access.member.type->as.base.is_unsigned;
    } else {
        if (access.member.offset > (size_t)INT_MAX ||
            symbol->location.address > INT_MAX - (int)access.member.offset) {
            return fail(table, "structure member address exceeds supported range",
                        expression->field);
        }
        access.location = symbol->location;
        access.location.address += (int)access.member.offset;
        access.location.width = access.member.width;
        access.location.is_unsigned =
            access.member.type->as.base.is_unsigned;
    }
    access.is_modifiable =
        (object_type->qualifiers & AST_QUALIFIER_CONST) == 0 &&
        (access.member.type->qualifiers & AST_QUALIFIER_CONST) == 0;
    grown = (MemberBinding *)realloc(table->member_bindings,
        (table->member_binding_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return fail(table, "out of memory resolving structure member",
                    expression->field);
    }
    table->member_bindings = grown;
    grown[table->member_binding_count].expression = expression;
    grown[table->member_binding_count].function = table->function;
    grown[table->member_binding_count++].access = access;
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

static int compound_base_operator(AstBinaryOp op, AstBinaryOp *base)
{
    switch (op) {
    case AST_BINARY_MUL_ASSIGN: *base = AST_BINARY_MUL; return 1;
    case AST_BINARY_DIV_ASSIGN: *base = AST_BINARY_DIV; return 1;
    case AST_BINARY_MOD_ASSIGN: *base = AST_BINARY_MOD; return 1;
    case AST_BINARY_ADD_ASSIGN: *base = AST_BINARY_ADD; return 1;
    case AST_BINARY_SUB_ASSIGN: *base = AST_BINARY_SUB; return 1;
    case AST_BINARY_SHIFT_LEFT_ASSIGN: *base = AST_BINARY_SHIFT_LEFT; return 1;
    case AST_BINARY_SHIFT_RIGHT_ASSIGN: *base = AST_BINARY_SHIFT_RIGHT; return 1;
    case AST_BINARY_AND_ASSIGN: *base = AST_BINARY_BIT_AND; return 1;
    case AST_BINARY_XOR_ASSIGN: *base = AST_BINARY_BIT_XOR; return 1;
    case AST_BINARY_OR_ASSIGN: *base = AST_BINARY_BIT_OR; return 1;
    default: return 0;
    }
}

static void init_synthetic_binary(AstBinaryExpr *binary, const AstExpr *origin,
                                  AstBinaryOp op, AstExpr *left,
                                  AstExpr *right)
{
    memset(binary, 0, sizeof(*binary));
    binary->expression.node.kind = AST_NODE_BINARY_EXPR;
    binary->expression.node.span = origin->node.span;
    binary->op = op;
    binary->left = left;
    binary->right = right;
}

static void init_synthetic_constant(AstConstantExpr *constant,
                                    const AstExpr *origin, size_t value)
{
    memset(constant, 0, sizeof(*constant));
    constant->expression.node.kind = AST_NODE_CONSTANT_EXPR;
    constant->expression.node.span = origin->node.span;
    constant->constant_kind = AST_CONSTANT_INTEGER;
    constant->value.integer = (long)value;
}

int symtab_update_rewrite(const ScalarSymbols *table,
                          const AstExpr *expression,
                          SymtabUpdateRewrite *rewrite)
{
    AstBinaryOp base;
    AstExpr *lvalue;
    AstExpr *operand = NULL;
    const AstType *pointee;
    size_t unit = 1;
    int increment = 0;

    if (expression == NULL || rewrite == NULL) {
        return 0;
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        if (unary->op < AST_UNARY_PRE_INCREMENT) {
            return 0;
        }
        lvalue = unary->operand;
        base = unary->op == AST_UNARY_PRE_INCREMENT ||
               unary->op == AST_UNARY_POST_INCREMENT
            ? AST_BINARY_ADD : AST_BINARY_SUB;
        rewrite->postfix = unary->op == AST_UNARY_POST_INCREMENT ||
                           unary->op == AST_UNARY_POST_DECREMENT;
        increment = 1;
    } else if (expression->node.kind == AST_NODE_BINARY_EXPR &&
               compound_base_operator(((const AstBinaryExpr *)expression)->op,
                                      &base)) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        lvalue = binary->left;
        operand = binary->right;
        rewrite->postfix = 0;
    } else {
        return 0;
    }
    rewrite->lvalue = lvalue;
    pointee = expression_pointer_pointee(table, lvalue);
    rewrite->is_pointer = pointee != NULL;
    if (pointee != NULL) {
        if (pointee->kind == AST_TYPE_STRUCT) {
            if (symtab_structure_size(table,
                    pointee->as.structure.declaration, &unit) != 0) {
                return -1;
            }
        } else {
            unit = symtab_type_width(pointee);
        }
        if (unit == 0) {
            return -1;
        }
    }
    init_synthetic_constant(&rewrite->step, expression, increment ? unit : 1);
    init_synthetic_constant(&rewrite->factor, expression, unit);
    if (increment) {
        operand = (AstExpr *)&rewrite->step;
    } else if (unit > 1) {
        init_synthetic_binary(&rewrite->scale, expression, AST_BINARY_MUL,
                              operand, (AstExpr *)&rewrite->factor);
        operand = (AstExpr *)&rewrite->scale;
    }
    init_synthetic_binary(&rewrite->operation, expression, base, lvalue,
                          operand);
    init_synthetic_binary(&rewrite->assign, expression, AST_BINARY_ASSIGN,
                          lvalue, (AstExpr *)&rewrite->operation);
    return 1;
}

static int expression_has_side_effects(const AstExpr *expression)
{
    AstBinaryOp base;

    if (expression == NULL) {
        return 0;
    }
    switch (expression->node.kind) {
    case AST_NODE_CALL_EXPR:
        return 1;
    case AST_NODE_BINARY_EXPR: {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        return binary->op == AST_BINARY_ASSIGN ||
               compound_base_operator(binary->op, &base) ||
               expression_has_side_effects(binary->left) ||
               expression_has_side_effects(binary->right);
    }
    case AST_NODE_UNARY_EXPR: {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        return unary->op >= AST_UNARY_PRE_INCREMENT ||
               expression_has_side_effects(unary->operand);
    }
    case AST_NODE_INDEX_EXPR:
        return expression_has_side_effects(
                   ((const AstIndexExpr *)expression)->array) ||
               expression_has_side_effects(
                   ((const AstIndexExpr *)expression)->index);
    case AST_NODE_MEMBER_EXPR:
        return expression_has_side_effects(
            ((const AstMemberExpr *)expression)->object);
    case AST_NODE_DEREFERENCE_EXPR:
        return expression_has_side_effects(
            ((const AstDereferenceExpr *)expression)->operand);
    case AST_NODE_ADDRESS_EXPR:
        return expression_has_side_effects(
            ((const AstAddressExpr *)expression)->operand);
    default:
        return 0;
    }
}

static int resolve_update(ScalarSymbols *table, SymtabUpdateRewrite *rewrite)
{
    if (expression_has_side_effects(rewrite->lvalue)) {
        return fail(table,
            "compound assignment and increment require a side-effect-free lvalue",
            NULL);
    }
    if (!rewrite->is_pointer) {
        return resolve_expression(table, &rewrite->assign.expression);
    }
    {
        const AstType *type;
        const ScalarSymbol *symbol;
        const AstExpr *operand = rewrite->operation.right;

        if (rewrite->lvalue->node.kind != AST_NODE_IDENT_EXPR ||
            resolve_expression(table, rewrite->lvalue) != 0) {
            return fail(table, "pointer update requires a pointer variable", NULL);
        }
        symbol = symtab_identifier(table, rewrite->lvalue);
        type = symbol == NULL ? NULL : symbol->declaration != NULL
            ? symbol->declaration->type : symbol->parameter->type;
        if (type == NULL || (type->qualifiers & AST_QUALIFIER_CONST) != 0) {
            return fail(table, "assignment requires a modifiable scalar lvalue", NULL);
        }
        if (rewrite->operation.op != AST_BINARY_ADD &&
            rewrite->operation.op != AST_BINARY_SUB) {
            return fail(table,
                "pointer arithmetic and other pointer operators are not supported",
                NULL);
        }
        if (operand == (const AstExpr *)&rewrite->scale) {
            operand = rewrite->scale.left;
        }
        if (operand != (const AstExpr *)&rewrite->step &&
            (expression_contains_void_call(table, operand) ||
             resolve_expression(table, operand) != 0 ||
             expression_is_pointer(table, operand))) {
            return fail(table, "pointer update requires an integer operand", NULL);
        }
    }
    return 0;
}

static int compatible_pointee_type(const AstType *left, const AstType *right)
{
    if (left == NULL || right == NULL || left->kind != right->kind ||
        left->qualifiers != right->qualifiers) {
        return 0;
    }
    if (left->kind == AST_TYPE_STRUCT) {
        return left->as.structure.declaration ==
               right->as.structure.declaration;
    }
    return left->kind == AST_TYPE_BASE &&
           left->as.base.base == right->as.base.base &&
           left->as.base.is_unsigned == right->as.base.is_unsigned;
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
        if (table->symbols[symbol_index].is_array) {
            return fail(table, "array value requires an index", name);
        }
        if (table->symbols[symbol_index].is_aggregate) {
            return fail(table, "aggregate value is not supported yet", name);
        }
        return 0;
    }
    if (expression->node.kind == AST_NODE_MEMBER_EXPR) {
        return bind_member_access(table, (const AstMemberExpr *)expression);
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
    if (expression->node.kind == AST_NODE_UNARY_EXPR ||
        expression->node.kind == AST_NODE_BINARY_EXPR) {
        SymtabUpdateRewrite rewrite;
        const AstExpr *target = expression->node.kind == AST_NODE_UNARY_EXPR
            ? ((const AstUnaryExpr *)expression)->operand
            : ((const AstBinaryExpr *)expression)->left;
        AstBinaryOp base;
        int update;

        if (target != NULL && target->node.kind == AST_NODE_IDENT_EXPR &&
            (expression->node.kind == AST_NODE_UNARY_EXPR
                ? ((const AstUnaryExpr *)expression)->op >= AST_UNARY_PRE_INCREMENT
                : compound_base_operator(
                      ((const AstBinaryExpr *)expression)->op, &base)) &&
            resolve_expression(table, target) != 0) {
            return -1;
        }
        update = symtab_update_rewrite(table, expression, &rewrite);
        if (update < 0) {
            return fail(table, "unsupported pointer update", NULL);
        }
        if (update > 0) {
            return resolve_update(table, &rewrite);
        }
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        if (unary->op != AST_UNARY_PLUS && unary->op != AST_UNARY_MINUS &&
            unary->op != AST_UNARY_LOGICAL_NOT &&
            unary->op != AST_UNARY_BITWISE_NOT) {
            return fail(table, "unsupported scalar operator", NULL);
        }
        if (expression_contains_void_call(table, unary->operand)) {
            return fail(table, "void function call used as a value", NULL);
        }
        if (resolve_expression(table, unary->operand) != 0) {
            return -1;
        }
        if (expression_is_pointer(table, unary->operand)) {
            return fail(table, "pointer arithmetic and other pointer operators are not supported",
                        NULL);
        }
        return 0;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op != AST_BINARY_ADD && binary->op != AST_BINARY_SUB &&
            binary->op != AST_BINARY_BIT_AND &&
            binary->op != AST_BINARY_BIT_OR &&
            binary->op != AST_BINARY_BIT_XOR &&
            binary->op != AST_BINARY_SHIFT_LEFT &&
            binary->op != AST_BINARY_SHIFT_RIGHT &&
            binary->op != AST_BINARY_MUL &&
            binary->op != AST_BINARY_DIV &&
            binary->op != AST_BINARY_MOD &&
            binary->op != AST_BINARY_LOGICAL_AND &&
            binary->op != AST_BINARY_LOGICAL_OR &&
            binary->op != AST_BINARY_COMMA &&
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
            int member_modifiable = 1;
            int modifiable;

            if (binary->left->node.kind == AST_NODE_IDENT_EXPR) {
                symbol = symtab_identifier(table, binary->left);
            } else if (binary->left->node.kind == AST_NODE_INDEX_EXPR) {
                symbol = symtab_identifier(table,
                    ((const AstIndexExpr *)binary->left)->array);
            } else if (binary->left->node.kind == AST_NODE_MEMBER_EXPR) {
                ResolvedMemberAccess resolved;
                if (symtab_member_access(table,
                        (const AstMemberExpr *)binary->left,
                        &resolved) == 0) {
                    target_type = resolved.member.type;
                    member_modifiable = resolved.is_modifiable;
                } else {
                    member_modifiable = 0;
                }
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
                member_modifiable &&
                (binary->left->node.kind == AST_NODE_IDENT_EXPR ||
                 binary->left->node.kind == AST_NODE_INDEX_EXPR ||
                 binary->left->node.kind == AST_NODE_MEMBER_EXPR ||
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
        if (binary->op == AST_BINARY_DIV || binary->op == AST_BINARY_MOD) {
            unsigned long divisor;
            if (symtab_initializer_constant(binary->right, &divisor) == 0 &&
                divisor == 0) {
                return fail(table, "division by constant zero", NULL);
            }
            if (!expression_unsigned_word(table, binary->left) &&
                !expression_unsigned_word(table, binary->right) &&
                !(expression_nonnegative(table, binary->left) &&
                  expression_nonnegative(table, binary->right))) {
                return fail(table, "signed division is not supported", NULL);
            }
        }
        if (binary->op == AST_BINARY_SHIFT_RIGHT &&
            !expression_nonnegative(table, binary->left)) {
            return fail(table, "signed right shift is not supported", NULL);
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
        expression->node.kind == AST_NODE_INDEX_EXPR ||
        expression->node.kind == AST_NODE_MEMBER_EXPR) {
        if (expression->node.kind == AST_NODE_MEMBER_EXPR) {
            ResolvedMemberAccess access;
            return symtab_member_access(table,
                (const AstMemberExpr *)expression, &access) == 0 &&
                access.location.width == 2 && access.location.is_unsigned;
        }
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
    if (expression->node.kind == AST_NODE_CAST_EXPR) {
        const AstCastExpr *cast = (const AstCastExpr *)expression;
        return cast->type != NULL && cast->type->kind == AST_TYPE_BASE &&
               cast->type->as.base.base == AST_TYPE_INT &&
               cast->type->as.base.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_CALL_EXPR) {
        const AstCallExpr *call = (const AstCallExpr *)expression;
        const AstIdentExpr *identifier = (const AstIdentExpr *)call->function;
        const AstFunctionDecl *function =
            symtab_function(table, identifier->name);
        return function != NULL && function->return_type->kind == AST_TYPE_BASE &&
               function->return_type->as.base.base == AST_TYPE_INT &&
               function->return_type->as.base.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        return unary->op != AST_UNARY_LOGICAL_NOT &&
               expression_unsigned_word(table, unary->operand);
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op == AST_BINARY_ASSIGN) {
            return expression_unsigned_word(table, binary->left);
        }
        if (binary->op == AST_BINARY_COMMA) {
            return expression_unsigned_word(table, binary->right);
        }
        if (binary->op == AST_BINARY_SHIFT_LEFT ||
            binary->op == AST_BINARY_SHIFT_RIGHT) {
            return expression_unsigned_word(table, binary->left);
        }
        return expression_unsigned_word(table, binary->left) ||
               expression_unsigned_word(table, binary->right);
    }
    return 0;
}

static int expression_unsigned_byte(const ScalarSymbols *table,
                                  const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_DEREFERENCE_EXPR) {
        ResolvedDereference dereference;
        return symtab_dereference_info(table,
            (const AstDereferenceExpr *)expression, &dereference) == 0 &&
            dereference.width == 1 && dereference.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_MEMBER_EXPR) {
        ResolvedMemberAccess access;
        return symtab_member_access(table,
            (const AstMemberExpr *)expression, &access) == 0 &&
            access.location.width == 1 && access.location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR ||
        expression->node.kind == AST_NODE_INDEX_EXPR) {
        const AstExpr *identifier = expression->node.kind == AST_NODE_INDEX_EXPR
            ? ((const AstIndexExpr *)expression)->array : expression;
        const ScalarSymbol *symbol = symtab_identifier(table, identifier);
        return symbol != NULL && symbol->location.width == 1 &&
               symbol->location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_CAST_EXPR) {
        const AstCastExpr *cast = (const AstCastExpr *)expression;
        return cast->type != NULL && cast->type->kind == AST_TYPE_BASE &&
               cast->type->as.base.base == AST_TYPE_CHAR &&
               cast->type->as.base.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_CALL_EXPR) {
        const AstCallExpr *call = (const AstCallExpr *)expression;
        const AstIdentExpr *identifier = (const AstIdentExpr *)call->function;
        const AstFunctionDecl *function =
            symtab_function(table, identifier->name);
        return function != NULL && function->return_type->kind == AST_TYPE_BASE &&
               function->return_type->as.base.base == AST_TYPE_CHAR &&
               function->return_type->as.base.is_unsigned;
    }
    return 0;
}

int symtab_switch_width(const ScalarSymbols *table, const AstExpr *expression,
                        size_t *width)
{
    if (expression == NULL) {
        return -1;
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR &&
        ((const AstUnaryExpr *)expression)->op == AST_UNARY_LOGICAL_NOT) {
        return -1;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        AstBinaryOp op = ((const AstBinaryExpr *)expression)->op;
        if (op == AST_BINARY_LESS || op == AST_BINARY_LESS_EQUAL ||
            op == AST_BINARY_GREATER || op == AST_BINARY_GREATER_EQUAL ||
            op == AST_BINARY_EQUAL || op == AST_BINARY_NOT_EQUAL ||
            op == AST_BINARY_LOGICAL_AND || op == AST_BINARY_LOGICAL_OR) {
            return -1;
        }
    }
    if (table == NULL || expression == NULL || width == NULL) {
        return -1;
    }
    if (expression_unsigned_byte(table, expression)) {
        *width = 1;
        return 0;
    }
    if (expression_unsigned_word(table, expression)) {
        *width = 2;
        return 0;
    }
    return -1;
}

static int expression_nonnegative(const ScalarSymbols *table,
                                 const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_UNARY_EXPR ||
        expression->node.kind == AST_NODE_BINARY_EXPR) {
        SymtabUpdateRewrite rewrite;
        if (symtab_update_rewrite(table, expression, &rewrite) > 0) {
            return expression_nonnegative(table, rewrite.lvalue);
        }
    }
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
    if (expression->node.kind == AST_NODE_MEMBER_EXPR) {
        ResolvedMemberAccess access;
        return symtab_member_access(table,
            (const AstMemberExpr *)expression, &access) == 0 &&
            access.location.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_DEREFERENCE_EXPR) {
        ResolvedDereference dereference;
        return symtab_dereference_info(table,
            (const AstDereferenceExpr *)expression, &dereference) == 0 &&
            dereference.is_unsigned;
    }
    if (expression->node.kind == AST_NODE_UNARY_EXPR) {
        const AstUnaryExpr *unary = (const AstUnaryExpr *)expression;
        return unary->op == AST_UNARY_LOGICAL_NOT ||
               (unary->op == AST_UNARY_PLUS &&
                expression_nonnegative(table, unary->operand));
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        if (binary->op == AST_BINARY_ASSIGN) {
            return expression_nonnegative(table, binary->left) &&
                   expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_BIT_AND) {
            return expression_nonnegative(table, binary->left) ||
                   expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_BIT_OR ||
            binary->op == AST_BINARY_BIT_XOR) {
            return expression_nonnegative(table, binary->left) &&
                   expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_COMMA) {
            return expression_nonnegative(table, binary->right);
        }
        if (binary->op == AST_BINARY_LOGICAL_AND ||
            binary->op == AST_BINARY_LOGICAL_OR) {
            return 1;
        }
        if (binary->op == AST_BINARY_SHIFT_RIGHT) {
            return expression_nonnegative(table, binary->left);
        }
        if (binary->op == AST_BINARY_SHIFT_LEFT) {
            return expression_unsigned_word(table, binary->left);
        }
        if (binary->op == AST_BINARY_ADD || binary->op == AST_BINARY_MUL ||
            binary->op == AST_BINARY_DIV || binary->op == AST_BINARY_MOD) {
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

typedef struct SwitchLabels {
    unsigned long *values;
    size_t count;
    size_t capacity;
    unsigned char *seen;
    size_t defaults;
} SwitchLabels;

static int collect_switch_labels(ScalarSymbols *table, const AstStmt *statement,
                                 SwitchLabels *labels)
{
    if (statement == NULL) {
        return 0;
    }
    switch (statement->node.kind) {
    case AST_NODE_SWITCH_STMT:
        return 0;
    case AST_NODE_COMPOUND_STMT: {
        const AstCompoundStmt *block = (const AstCompoundStmt *)statement;
        size_t index;
        for (index = 0; index < block->item_count; ++index) {
            const AstBlockItem *item = &block->items[index];
            if (item->kind == AST_BLOCK_STATEMENT &&
                collect_switch_labels(table, item->as.statement, labels) != 0) {
                return -1;
            }
        }
        return 0;
    }
    case AST_NODE_CASE_STMT: {
        const AstCaseStmt *case_statement = (const AstCaseStmt *)statement;
        unsigned long value;
        if (symtab_initializer_constant(case_statement->value, &value) != 0 ||
            (!expression_nonnegative(table, case_statement->value) &&
             !expression_unsigned_word(table, case_statement->value))) {
            return fail(table, "case label is not a supported nonnegative integer constant",
                        NULL);
        }
        if (value > 65535UL ||
            (labels->seen[value >> 3] & (unsigned char)(1u << (value & 7))) != 0) {
            return fail(table, "duplicate case value", NULL);
        }
        labels->seen[value >> 3] |= (unsigned char)(1u << (value & 7));
        if (labels->count == labels->capacity) {
            size_t capacity = labels->capacity == 0 ? 8 : labels->capacity * 2;
            unsigned long *values;
            if (capacity < labels->capacity ||
                capacity > (size_t)-1 / sizeof(*values)) {
                return fail(table, "too many switch case labels", NULL);
            }
            values = (unsigned long *)realloc(labels->values,
                                               capacity * sizeof(*values));
            if (values == NULL) {
                return fail(table, "out of memory validating switch labels", NULL);
            }
            labels->values = values;
            labels->capacity = capacity;
        }
        labels->values[labels->count++] = value;
        return collect_switch_labels(table, case_statement->body, labels);
    }
    case AST_NODE_DEFAULT_STMT: {
        const AstDefaultStmt *default_statement =
            (const AstDefaultStmt *)statement;
        if (++labels->defaults > 1) {
            return fail(table, "duplicate default label", NULL);
        }
        return collect_switch_labels(table, default_statement->body, labels);
    }
    case AST_NODE_IF_STMT: {
        const AstIfStmt *conditional = (const AstIfStmt *)statement;
        return collect_switch_labels(table, conditional->then_branch, labels) != 0 ||
               collect_switch_labels(table, conditional->else_branch, labels) != 0
            ? -1 : 0;
    }
    case AST_NODE_WHILE_STMT:
        return collect_switch_labels(table,
            ((const AstWhileStmt *)statement)->body, labels);
    case AST_NODE_DO_WHILE_STMT:
        return collect_switch_labels(table,
            ((const AstDoWhileStmt *)statement)->body, labels);
    case AST_NODE_FOR_STMT:
        return collect_switch_labels(table,
            ((const AstForStmt *)statement)->body, labels);
    case AST_NODE_LABEL_STMT:
        return collect_switch_labels(table,
            ((const AstLabelStmt *)statement)->body, labels);
    default:
        return 0;
    }
}

static int resolve_statement(ScalarSymbols *table, const AstStmt *statement)
{
    if (table->returned) {
        if (table->switch_depth == 0) {
            return fail(table, "statements after return are not supported yet", NULL);
        }
        table->returned = 0;
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
    case AST_NODE_SWITCH_STMT: {
        const AstSwitchStmt *selection = (const AstSwitchStmt *)statement;
        SwitchLabels labels = {0};
        int was_returned = table->returned;
        size_t switch_width;
        labels.seen = (unsigned char *)calloc(8192, 1);
        if (labels.seen == NULL) {
            return fail(table, "out of memory validating switch labels", NULL);
        }
        if (expression_contains_void_call(table, selection->expression) ||
            resolve_expression(table, selection->expression) != 0) {
            free(labels.values);
            free(labels.seen);
            return -1;
        }
        if (symtab_switch_width(table, selection->expression, &switch_width) != 0) {
            free(labels.values);
            free(labels.seen);
            return fail(table, "switch controlling expression must be unsigned",
                        NULL);
        }
        if (collect_switch_labels(table, selection->body, &labels) != 0) {
            free(labels.values);
            free(labels.seen);
            return -1;
        }
        if (switch_width == 1 && labels.count > 255) {
            free(labels.values);
            free(labels.seen);
            return fail(table, "byte switch exceeds the PASM CASE limit of 255 labels",
                        NULL);
        }
        {
            size_t index;
            unsigned char high_seen[256] = {0};
            unsigned int low_counts[256] = {0};
            size_t high_count = 0;
            for (index = 0; index < labels.count; ++index) {
                if (labels.values[index] > 65535UL) {
                    free(labels.values);
                    free(labels.seen);
                    return fail(table, "case value exceeds the target integer range",
                                NULL);
                }
                if (switch_width == 1 && labels.values[index] > 255UL) {
                    free(labels.values);
                    free(labels.seen);
                    return fail(table, "case value exceeds unsigned byte switch range",
                                NULL);
                }
                if (switch_width == 2) {
                    unsigned int high = (unsigned int)(labels.values[index] >> 8);
                    if (!high_seen[high]) {
                        high_seen[high] = 1;
                        ++high_count;
                    }
                    if (++low_counts[high] > 255) {
                        free(labels.values);
                        free(labels.seen);
                        return fail(table,
                            "word switch high-byte group exceeds the PASM CASE limit of 255 labels",
                            NULL);
                    }
                }
            }
            if (switch_width == 2 && high_count > 255) {
                free(labels.values);
                free(labels.seen);
                return fail(table,
                    "word switch exceeds the PASM high-byte CASE limit of 255 groups",
                    NULL);
            }
        }
        free(labels.values);
        free(labels.seen);
        ++table->switch_depth;
        if (resolve_statement(table, selection->body) != 0) {
            --table->switch_depth;
            return -1;
        }
        --table->switch_depth;
        table->returned = was_returned;
        return 0;
    }
    case AST_NODE_CASE_STMT: {
        int was_returned = table->returned;
        if (table->switch_depth == 0) {
            return fail(table, "case label used outside a switch", NULL);
        }
        if (resolve_statement(table, ((const AstCaseStmt *)statement)->body) != 0) {
            return -1;
        }
        table->returned = was_returned;
        return 0;
    }
    case AST_NODE_DEFAULT_STMT: {
        int was_returned = table->returned;
        if (table->switch_depth == 0) {
            return fail(table, "default label used outside a switch", NULL);
        }
        if (resolve_statement(table,
                ((const AstDefaultStmt *)statement)->body) != 0) {
            return -1;
        }
        table->returned = was_returned;
        return 0;
    }
    case AST_NODE_BREAK_STMT:
        if (table->loop_depth == 0 && table->switch_depth == 0) {
            return fail(table, "break or continue used outside a loop", NULL);
        }
        return 0;
    case AST_NODE_CONTINUE_STMT:
        if (table->loop_depth == 0) {
            return fail(table, "continue used outside a loop", NULL);
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
        for (local = 0; local < table->member_binding_count; ++local) {
            MemberBinding *binding = &table->member_bindings[local];
            if (binding->function == function &&
                binding->access.location.storage == SCALAR_LOCAL) {
                binding->access.location.address += (int)table->local_bytes;
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

int symtab_structure_size(const ScalarSymbols *table,
                          const AstStructDecl *structure, size_t *size)
{
    const StructureLayout *layout;
    if (table == NULL || structure == NULL || size == NULL) {
        return -1;
    }
    layout = find_structure_layout(table, structure);
    if (layout == NULL) {
        return -1;
    }
    *size = layout->size;
    return 0;
}

int symtab_structure_member(const ScalarSymbols *table,
                            const AstStructDecl *structure, const char *name,
                            ResolvedStructMember *member)
{
    const StructureLayout *layout;
    size_t index;
    if (table == NULL || structure == NULL || name == NULL || member == NULL) {
        return -1;
    }
    layout = find_structure_layout(table, structure);
    if (layout == NULL) {
        return -1;
    }
    for (index = 0; index < layout->member_count; ++index) {
        if (strcmp(layout->members[index].name, name) == 0) {
            *member = layout->members[index];
            return 0;
        }
    }
    return -1;
}

int symtab_member_access(const ScalarSymbols *table,
                         const AstMemberExpr *expression,
                         ResolvedMemberAccess *access)
{
    size_t index;
    if (table == NULL || expression == NULL || access == NULL) {
        return -1;
    }
    for (index = 0; index < table->member_binding_count; ++index) {
        if (table->member_bindings[index].expression == expression) {
            *access = table->member_bindings[index].access;
            return 0;
        }
    }
    return -1;
}
