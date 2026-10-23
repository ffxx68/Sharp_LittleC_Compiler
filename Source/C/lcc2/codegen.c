#include "codegen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CodegenContext {
    char *output;
    size_t output_length;
    size_t output_capacity;
    size_t stack_depth;
    unsigned long label_count;
    unsigned char library_used[101];
    char error[256];
};

static int set_error(CodegenContext *ctx, const char *message)
{
    if (ctx != NULL && ctx->error[0] == '\0') {
        (void)snprintf(ctx->error, sizeof(ctx->error), "%s", message);
    }
    return -1;
}

static int append_text(CodegenContext *ctx, const char *text, size_t length)
{
    size_t required;

    if (ctx == NULL || text == NULL) {
        return set_error(ctx, "invalid code generation context or text");
    }
    if (length > (size_t)-1 - ctx->output_length - 1) {
        return set_error(ctx, "assembly output size overflow");
    }
    required = ctx->output_length + length + 1;
    if (required > ctx->output_capacity) {
        size_t capacity = ctx->output_capacity == 0 ? 256 : ctx->output_capacity;
        char *output;

        while (capacity < required) {
            if (capacity > (size_t)-1 / 2) {
                capacity = required;
                break;
            }
            capacity *= 2;
        }
        output = (char *)realloc(ctx->output, capacity);
        if (output == NULL) {
            return set_error(ctx, "out of memory while growing assembly output");
        }
        ctx->output = output;
        ctx->output_capacity = capacity;
    }
    memcpy(ctx->output + ctx->output_length, text, length);
    ctx->output_length += length;
    ctx->output[ctx->output_length] = '\0';
    return 0;
}

static int append_format(CodegenContext *ctx, const char *format,
                         const char *first, const char *second,
                         const char *third)
{
    char line[512];
    int length;

    if (first == NULL) {
        first = "";
    }
    if (second == NULL) {
        second = "";
    }
    if (third == NULL) {
        third = "";
    }
    length = snprintf(line, sizeof(line), format, first, second, third);
    if (length < 0 || (size_t)length >= sizeof(line)) {
        return set_error(ctx, "assembly line exceeds the supported length");
    }
    return append_text(ctx, line, (size_t)length);
}

static void emit_inst_comment(CodegenContext *ctx, const char *inst,
                              const char *comment)
{
    if (ctx == NULL || ctx->error[0] != '\0' || inst == NULL ||
        comment == NULL) {
        return;
    }
    (void)append_format(ctx, "\t%s\t; %s\n", inst, comment, NULL);
}

CodegenContext *codegen_context_create(void)
{
    CodegenContext *ctx = (CodegenContext *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->output = (char *)calloc(1, 1);
    if (ctx->output == NULL) {
        free(ctx);
        return NULL;
    }
    ctx->output_capacity = 1;
    return ctx;
}

void codegen_context_destroy(CodegenContext *ctx)
{
    if (ctx != NULL) {
        free(ctx->output);
        free(ctx);
    }
}

const char *codegen_output(const CodegenContext *ctx)
{
    return ctx == NULL || ctx->output == NULL ? "" : ctx->output;
}

const char *codegen_error(const CodegenContext *ctx)
{
    return ctx == NULL ? "invalid code generation context" : ctx->error;
}

size_t codegen_stack_depth(const CodegenContext *ctx)
{
    return ctx == NULL ? 0 : ctx->stack_depth;
}

void emit_inst(CodegenContext *ctx, const char *inst)
{
    if (ctx == NULL || ctx->error[0] != '\0' || inst == NULL) {
        return;
    }
    (void)append_format(ctx, "\t%s\n", inst, NULL, NULL);
}

void emit_inst_op(CodegenContext *ctx, const char *inst, const char *operand)
{
    if (ctx == NULL || ctx->error[0] != '\0' || inst == NULL ||
        operand == NULL) {
        return;
    }
    (void)append_format(ctx, "\t%s\t%s\n", inst, operand, NULL);
}

void emit_inst_op_comment(CodegenContext *ctx, const char *inst,
                          const char *operand, const char *comment)
{
    if (ctx == NULL || ctx->error[0] != '\0' || inst == NULL ||
        operand == NULL || comment == NULL) {
        return;
    }
    if (operand[0] == '\0') {
        (void)append_format(ctx, "\t%s ; %s\n", inst, comment, NULL);
    } else {
        (void)append_format(ctx, "\t%s\t%s\t; %s\n", inst, operand, comment);
    }
}

void emit_comment(CodegenContext *ctx, const char *comment)
{
    if (ctx == NULL || ctx->error[0] != '\0' || comment == NULL) {
        return;
    }
    (void)append_format(ctx, "\t; %s\n", comment, NULL, NULL);
}

void emit_label(CodegenContext *ctx, const char *label)
{
    if (ctx == NULL || ctx->error[0] != '\0' || label == NULL) {
        return;
    }
    (void)append_format(ctx, "  %s:\n", label, NULL, NULL);
}

char *new_label(CodegenContext *ctx)
{
    char label[48];
    int length;

    if (ctx == NULL || ctx->error[0] != '\0') {
        return NULL;
    }
    length = snprintf(label, sizeof(label), "LB%lu", ctx->label_count++);
    if (length < 0 || (size_t)length >= sizeof(label)) {
        (void)set_error(ctx, "label counter exceeds supported range");
        return NULL;
    }
    {
        char *copy = (char *)malloc((size_t)length + 1);
        if (copy == NULL) {
            (void)set_error(ctx, "out of memory while allocating label");
            return NULL;
        }
        memcpy(copy, label, (size_t)length + 1);
        return copy;
    }
}

int codegen_emit_push(CodegenContext *ctx, size_t width)
{
    size_t index;

    if (ctx == NULL || (width != 1 && width != 2)) {
        return set_error(ctx, "push supports only byte and word values");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    for (index = 0; index < width; ++index) {
        if (width == 1) {
            emit_inst_op_comment(ctx, "PUSH", "", "byte");
        } else if (index == 0) {
            emit_inst_op_comment(ctx, "PUSH", "", "word (A, then B)");
        } else {
            emit_inst(ctx, "PUSH");
        }
        if (ctx->error[0] != '\0') {
            return -1;
        }
        ++ctx->stack_depth;
        if (width == 2 && index == 0) {
            emit_inst(ctx, "EXAB");
        }
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

int codegen_emit_pop(CodegenContext *ctx, size_t width)
{
    size_t index;

    if (ctx == NULL || (width != 1 && width != 2)) {
        return set_error(ctx, "pop supports only byte and word values");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    if (ctx->stack_depth < width) {
        return set_error(ctx, "evaluation stack underflow");
    }
    for (index = 0; index < width; ++index) {
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        if (width == 2 && index == 0) {
            emit_inst(ctx, "EXAB");
        }
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

int codegen_pop_add(CodegenContext *ctx, size_t width)
{
    if (ctx == NULL || (width != 1 && width != 2)) {
        return set_error(ctx, "addition supports only byte and word values");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    if (ctx->stack_depth < width) {
        return set_error(ctx, "evaluation stack underflow");
    }
    if (width == 1) {
        emit_inst(ctx, "EXAB");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst_op(ctx, "LP", "3");
        emit_inst_comment(ctx, "ADM", "Addition");
        emit_inst(ctx, "EXAB");
    } else {
        emit_inst_op(ctx, "LP", "0");
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, "EXAB");
        emit_inst_op(ctx, "LP", "1");
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst(ctx, "EXAB");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst_op(ctx, "LP", "0");
        emit_inst_comment(ctx, "ADB", "Addition");
        emit_inst_op(ctx, "LP", "1");
        emit_inst(ctx, "LDM");
        emit_inst(ctx, "EXAB");
        emit_inst_op(ctx, "LP", "0");
        emit_inst(ctx, "LDM");
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

int codegen_pop_sub(CodegenContext *ctx, size_t width)
{
    if (ctx == NULL || (width != 1 && width != 2)) {
        return set_error(ctx, "subtraction supports only byte and word values");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    if (ctx->stack_depth < width) {
        return set_error(ctx, "evaluation stack underflow");
    }
    if (width == 1) {
        emit_inst(ctx, "EXAB");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst_op(ctx, "LP", "3");
        emit_inst(ctx, "EXAB");
        emit_inst_comment(ctx, "SBM", "Subtraction");
        emit_inst(ctx, "EXAB");
    } else {
        emit_inst_op(ctx, "LP", "1");
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst(ctx, "EXAM");
        emit_inst_op(ctx, "LP", "0");
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, "POP");
        --ctx->stack_depth;
        emit_inst(ctx, "EXAM");
        emit_inst_comment(ctx, "SBB", "Subtraction");
        emit_inst_op(ctx, "LP", "1");
        emit_inst(ctx, "LDM");
        emit_inst(ctx, "EXAB");
        emit_inst_op(ctx, "LP", "0");
        emit_inst(ctx, "LDM");
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

int codegen_load_constant(CodegenContext *ctx, size_t width,
                          unsigned long value)
{
    char operand[32];
    char comment[64];
    int length;

    if (ctx == NULL || (width != 1 && width != 2)) {
        return set_error(ctx, "constant load supports only byte and word values");
    }
    if (value > (width == 1 ? 255UL : 65535UL)) {
        return set_error(ctx, "integer constant is outside the target width");
    }
    if (width == 1) {
        length = snprintf(operand, sizeof(operand), "%lu", value);
        if (length < 0 || (size_t)length >= sizeof(operand)) {
            return set_error(ctx, "cannot format integer constant");
        }
        (void)snprintf(comment, sizeof(comment), "Load byte constant %s",
                       operand);
        emit_inst_op_comment(ctx, "LIA", operand, comment);
        return ctx->error[0] == '\0' ? 0 : -1;
    }
    length = snprintf(operand, sizeof(operand), "%lu", value);
    if (length < 0 || (size_t)length >= sizeof(operand)) {
        return set_error(ctx, "cannot format integer constant");
    }
    {
        char low[40];
        char high[40];
        (void)snprintf(low, sizeof(low), "LB(%s)", operand);
        (void)snprintf(high, sizeof(high), "HB(%s)", operand);
        emit_inst_op_comment(ctx, "LIA", low, "Load word constant LB");
        emit_inst_op_comment(ctx, "LIB", high, "Load word constant HB");
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

static size_t expression_width(const AstExpr *expression);

static int validate_expression(const AstExpr *expression)
{
    if (expression == NULL) {
        return 0;
    }
    switch (expression->node.kind) {
    case AST_NODE_CONSTANT_EXPR: {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        size_t width = expression_width(expression);
        unsigned long value;
        if (constant->constant_kind == AST_CONSTANT_INTEGER) {
            if (constant->value.integer < 0) {
                return 0;
            }
            value = (unsigned long)constant->value.integer;
        } else if (constant->constant_kind == AST_CONSTANT_CHARACTER) {
            value = constant->value.character;
        } else {
            return 0;
        }
        return value <= (width == 1 ? 255UL : 65535UL);
    }
    case AST_NODE_BINARY_EXPR: {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        return (binary->op == AST_BINARY_ADD ||
                binary->op == AST_BINARY_SUB) &&
               expression_width(binary->left) == 2 &&
               expression_width(binary->right) == 2 &&
               validate_expression(binary->left) &&
               validate_expression(binary->right);
    }
    default:
        return 0;
    }
}

static size_t expression_width(const AstExpr *expression)
{
    if (expression != NULL &&
        expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        if (constant->type != NULL &&
            constant->type->kind == AST_TYPE_BASE &&
            constant->type->as.base.base == AST_TYPE_CHAR) {
            return 1;
        }
    }
    return 2;
}

static int emit_expression(CodegenContext *ctx, const AstExpr *expression)
{
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        unsigned long value;

        if (constant->constant_kind == AST_CONSTANT_CHARACTER) {
            value = constant->value.character;
        } else {
            value = (unsigned long)constant->value.integer;
        }
        return codegen_load_constant(ctx, expression_width(expression), value);
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        size_t width = 2;

        if (emit_expression(ctx, binary->left) != 0 ||
            codegen_emit_push(ctx, width) != 0 ||
            emit_expression(ctx, binary->right) != 0) {
            return -1;
        }
        return binary->op == AST_BINARY_ADD
                   ? codegen_pop_add(ctx, width)
                   : codegen_pop_sub(ctx, width);
    }
    return set_error(ctx, "unsupported expression reached during code generation");
}

static int validate_function(const AstFunctionDecl *function)
{
    AstStmt *statement;
    AstType *return_type;

    if (function->is_prototype) {
        return 1;
    }
    return_type = function->return_type;
    if (return_type == NULL || return_type->kind != AST_TYPE_BASE ||
        (return_type->as.base.base != AST_TYPE_VOID &&
         return_type->as.base.base != AST_TYPE_CHAR &&
         return_type->as.base.base != AST_TYPE_INT)) {
        return 0;
    }
    if (function->parameter_count != 0 || function->body == NULL ||
        function->body->item_count > 1) {
        return 0;
    }
    if (function->body->item_count == 0) {
        return return_type->as.base.base == AST_TYPE_VOID;
    }
    if (function->body->items[0].kind != AST_BLOCK_STATEMENT) {
        return 0;
    }
    statement = function->body->items[0].as.statement;
    if (statement == NULL || statement->node.kind != AST_NODE_RETURN_STMT) {
        return 0;
    }
    {
        AstReturnStmt *return_statement = (AstReturnStmt *)statement;
        if (return_statement->expression == NULL) {
            return return_type->as.base.base == AST_TYPE_VOID;
        }
        return return_type->as.base.base != AST_TYPE_VOID &&
               expression_width(return_statement->expression) ==
                   (return_type->as.base.base == AST_TYPE_CHAR ? 1u : 2u) &&
               validate_expression(return_statement->expression);
    }
}

static int validate_translation_unit(CodegenContext *ctx, AstTranslationUnit *tu)
{
    size_t index;

    if (ctx == NULL || tu == NULL) {
        return set_error(ctx, "invalid translation unit");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    for (index = 0; index < tu->declarations.count; ++index) {
        AstDecl *declaration = tu->declarations.items[index];
        if (declaration->node.kind != AST_NODE_FUNCTION_DECL ||
            !validate_function((AstFunctionDecl *)declaration)) {
            return set_error(ctx,
                "unsupported translation unit construct (only parameterless "
                "functions returning constants or + / - expressions are supported)");
        }
    }
    return 0;
}

int codegen_translation_unit(CodegenContext *ctx, AstTranslationUnit *tu)
{
    size_t index;

    if (validate_translation_unit(ctx, tu) != 0) {
        return -1;
    }
    for (index = 0; index < tu->declarations.count; ++index) {
        AstFunctionDecl *function =
            (AstFunctionDecl *)tu->declarations.items[index];
        AstReturnStmt *return_statement = NULL;
        if (function->is_prototype) {
            continue;
        }
        if (ctx->stack_depth != 0) {
            return set_error(ctx, "evaluation stack is not empty at function entry");
        }
        (void)append_format(ctx, "%s:\t; Procedure\n", function->name, NULL,
                            NULL);
        if (function->body->item_count != 0) {
            return_statement = (AstReturnStmt *)
                function->body->items[0].as.statement;
            if (return_statement->expression != NULL &&
                emit_expression(ctx, return_statement->expression) != 0) {
                return -1;
            }
        }
        emit_inst(ctx, "RTN");
        (void)append_text(ctx, "\n", 1);
        if (ctx->error[0] != '\0') {
            return -1;
        }
        if (ctx->stack_depth != 0) {
            return set_error(ctx, "evaluation stack is unbalanced at function exit");
        }
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

static int origin_value(const AstExpr *expression, unsigned long *value)
{
    if (expression->node.kind == AST_NODE_CONSTANT_EXPR) {
        const AstConstantExpr *constant = (const AstConstantExpr *)expression;
        if (constant->constant_kind != AST_CONSTANT_INTEGER ||
            constant->value.integer < 0 || constant->value.integer > 65535) {
            return -1;
        }
        *value = (unsigned long)constant->value.integer;
        return 0;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        unsigned long left, right;
        if (origin_value(binary->left, &left) != 0 ||
            origin_value(binary->right, &right) != 0) {
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

static int same_assembler_name(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (toupper((unsigned char)*left++) != toupper((unsigned char)*right++)) {
            return 0;
        }
    }
    return *left == *right;
}

int codegen_program(CodegenContext *ctx, AstTranslationUnit *tu)
{
    unsigned long origin = 0xE030;
    size_t index, other;
    int has_main = 0;
    char operand[32];

    if (validate_translation_unit(ctx, tu) != 0) {
        return -1;
    }
    if (ctx->output_length != 0 || ctx->stack_depth != 0) {
        return set_error(ctx, "program generation requires an empty context");
    }
    if (tu->has_pc_pragma) {
        return set_error(ctx, "#pragma pc is not supported by program generation yet");
    }
    if (tu->origin != NULL && origin_value(tu->origin, &origin) != 0) {
        return set_error(ctx,
            "#pragma org requires a 16-bit nonnegative integer constant or + / - expression");
    }
    for (index = 0; index < tu->declarations.count; ++index) {
        AstFunctionDecl *function = (AstFunctionDecl *)tu->declarations.items[index];
        if (!tu->no_save && same_assembler_name(function->name, "SREG")) {
            return set_error(ctx, "SREG is reserved for the register-save wrapper");
        }
        if (function->is_prototype) {
            continue;
        }
        if (strcmp(function->name, "main") == 0) {
            has_main = 1;
        }
        for (other = 0; other < index; ++other) {
            AstFunctionDecl *previous = (AstFunctionDecl *)tu->declarations.items[other];
            if (!previous->is_prototype &&
                same_assembler_name(function->name, previous->name)) {
                return set_error(ctx, "function definitions collide in PASM's case-insensitive namespace");
            }
        }
    }
    if (!has_main) {
        return set_error(ctx, "program requires a parameterless main definition");
    }
    emit_comment(ctx, "pasm file - assemble with pasm!");
    emit_comment(ctx, "Compiled with lcc2");
    (void)snprintf(operand, sizeof(operand), "%lu", origin);
    (void)append_format(ctx, ".ORG\t%s\n\n", operand, NULL, NULL);
    if (!tu->no_save) {
        emit_inst_op(ctx, "LP", "0");
        emit_inst_op(ctx, "LIDP", "SREG");
        emit_inst_op(ctx, "LII", "11");
        emit_inst(ctx, "EXWD");
    }
    emit_inst_op(ctx, "CALL", "main");
    if (!tu->no_save) {
        emit_inst_op(ctx, "LP", "0");
        emit_inst_op(ctx, "LIDP", "SREG");
        emit_inst_op(ctx, "LII", "11");
        emit_inst(ctx, "MVWD");
    }
    emit_inst(ctx, "RTN");
    if (!tu->no_save) {
        (void)append_text(ctx, "SREG:\t.DW 0, 0, 0, 0, 0, 0\n\n",
                          strlen("SREG:\t.DW 0, 0, 0, 0, 0, 0\n\n"));
    }
    return codegen_translation_unit(ctx, tu);
}
