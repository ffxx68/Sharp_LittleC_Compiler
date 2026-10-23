#include "codegen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LoopLabels {
    const char *break_label;
    const char *continue_label;
} LoopLabels;

/* Unsigned B:A versus J:I comparisons from the legacy .lib files.
   Results are 0 or 255 in A; the caller clears B. */
static const struct {
    const char *name;
    const char *labels[3];
    const char *source;
} runtime_routines[] = {
    {"LIB_CPE16", {"LIB_CPE16", "CPQ1", NULL},
     "LIB_CPE16:\n\tLP\t0\n\tCPMA\n\tJRNZP\tCPQ1\n"
     "\tLP\t1\n\tEXAB\n\tCPMA\n\tJRNZP\tCPQ1\n"
     "\tLIA\t255\n\tRTN\nCPQ1:\n\tLIA\t0\n\tRTN\n"},
    {"LIB_CPNE16", {"LIB_CPNE16", "CPN1", NULL},
     "LIB_CPNE16:\n\tLP\t0\n\tCPMA\n\tJRNZP\tCPN1\n"
     "\tLP\t1\n\tEXAB\n\tCPMA\n\tJRNZP\tCPN1\n"
     "\tLIA\t0\n\tRTN\nCPN1:\n\tLIA\t255\n\tRTN\n"},
    {"LIB_CPS16", {"LIB_CPS16", "CPS0", "CPS1"},
     "LIB_CPS16:\n\tEXAB\n\tLP\t1\n\tEXAM\n\tCPMA\n"
     "\tJRCP\tCPS1\n\tJRNZP\tCPS0\n\tLP\t0\n\tEXAB\n"
     "\tEXAM\n\tCPMA\n\tJRCP\tCPS1\nCPS0:\n\tLIA\t0\n"
     "\tRTN\nCPS1:\n\tLIA\t255\n\tRTN\n"},
    {"LIB_CPSE16", {"LIB_CPSE16", "CPSE0", "CPSE1"},
     "LIB_CPSE16:\n\tEXAB\n\tLP\t1\n\tEXAM\n\tCPMA\n"
     "\tJRCP\tCPSE1\n\tJRNZP\tCPSE0\n\tLP\t0\n\tEXAB\n"
     "\tCPMA\n\tJRNCP\tCPSE1\nCPSE0:\n\tLIA\t0\n"
     "\tRTN\nCPSE1:\n\tLIA\t255\n\tRTN\n"},
    {"LIB_CPG16", {"LIB_CPG16", "CPG0", "CPG1"},
     "LIB_CPG16:\n\tEXAB\n\tLP\t1\n\tCPMA\n\tJRCP\tCPG1\n"
     "\tJRNZP\tCPG0\n\tLP\t0\n\tEXAB\n\tCPMA\n\tJRCP\tCPG1\n"
     "CPG0:\n\tLIA\t0\n\tRTN\nCPG1:\n\tLIA\t255\n\tRTN\n"},
    {"LIB_CPGE16", {"LIB_CPGE16", "CPGE0", "CPGE1"},
     "LIB_CPGE16:\n\tEXAB\n\tLP\t1\n\tCPMA\n\tJRCP\tCPGE1\n"
     "\tJRNZP\tCPGE0\n\tLP\t0\n\tEXAB\n\tEXAM\n\tCPMA\n"
     "\tJRNCP\tCPGE1\nCPGE0:\n\tLIA\t0\n\tRTN\n"
     "CPGE1:\n\tLIA\t255\n\tRTN\n"}
};

#define RUNTIME_COUNT (sizeof(runtime_routines) / sizeof(runtime_routines[0]))

struct CodegenContext {
    char *output;
    size_t output_length;
    size_t output_capacity;
    size_t stack_depth;
    size_t frame_depth;
    unsigned long label_count;
    unsigned char library_used[RUNTIME_COUNT];
    char *runtime_output;
    char *startup_output;
    char *function_output;
    char *data_output;
    ScalarSymbols *symbols;
    AstTranslationUnit *resolved_unit;
    char *return_label;
    const AstType *return_type;
    LoopLabels *loops;
    size_t loop_count;
    size_t loop_capacity;
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
        symtab_destroy(ctx->symbols);
        free(ctx->return_label);
        free(ctx->loops);
        free(ctx->output);
        free(ctx->runtime_output);
        free(ctx->startup_output);
        free(ctx->function_output);
        free(ctx->data_output);
        free(ctx);
    }
}

const char *codegen_output(const CodegenContext *ctx)
{
    return ctx == NULL || ctx->output == NULL ? "" : ctx->output;
}

const char *codegen_runtime_output(const CodegenContext *ctx)
{
    return ctx == NULL || ctx->runtime_output == NULL ? "" : ctx->runtime_output;
}

const char *codegen_section_output(const CodegenContext *ctx,
                                   CodegenSection section)
{
    const char *text = NULL;
    if (ctx != NULL) {
        switch (section) {
        case CODEGEN_STARTUP: text = ctx->startup_output; break;
        case CODEGEN_FUNCTIONS: text = ctx->function_output; break;
        case CODEGEN_DATA: text = ctx->data_output; break;
        case CODEGEN_RUNTIME: text = ctx->runtime_output; break;
        }
    }
    return text == NULL ? "" : text;
}

static char *take_output(CodegenContext *ctx)
{
    char *text = ctx->output;
    ctx->output = NULL;
    ctx->output_length = 0;
    ctx->output_capacity = 0;
    return text;
}

int codegen_require_runtime(CodegenContext *ctx, const char *routine)
{
    size_t index;
    char message[256];
    if (ctx == NULL || routine == NULL) {
        return set_error(ctx, "invalid runtime routine request");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    for (index = 0; index < RUNTIME_COUNT; ++index) {
        if (strcmp(routine, runtime_routines[index].name) == 0) {
            ctx->library_used[index] = 1;
            return 0;
        }
    }
    (void)snprintf(message, sizeof(message),
                   "unresolved runtime routine: %s", routine);
    return set_error(ctx, message);
}

static int resolve_runtime(CodegenContext *ctx)
{
    size_t index, length = 0;
    char *output, *next;
    for (index = 0; index < RUNTIME_COUNT; ++index) {
        if (ctx->library_used[index]) {
            length += strlen(runtime_routines[index].source) + 1;
        }
    }
    output = (char *)malloc(length + 1);
    if (output == NULL) {
        return set_error(ctx, "out of memory while including runtime routines");
    }
    next = output;
    for (index = 0; index < RUNTIME_COUNT; ++index) {
        if (ctx->library_used[index]) {
            size_t size = strlen(runtime_routines[index].source);
            memcpy(next, runtime_routines[index].source, size);
            next += size;
            *next++ = '\n';
        }
    }
    *next = '\0';
    free(ctx->runtime_output);
    ctx->runtime_output = output;
    return 0;
}

const char *codegen_error(const CodegenContext *ctx)
{
    return ctx == NULL ? "invalid code generation context" : ctx->error;
}

size_t codegen_stack_depth(const CodegenContext *ctx)
{
    return ctx == NULL ? 0 : ctx->stack_depth + ctx->frame_depth;
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

static void emit_number(CodegenContext *ctx, const char *instruction, int value)
{
    char operand[32];
    (void)snprintf(operand, sizeof(operand), "%d", value);
    emit_inst_op(ctx, instruction, operand);
}

static int scalar_offset(CodegenContext *ctx, ScalarLocation location, int store)
{
    int extra = location.width == 2 && !store ? 1 : 2;
    if (ctx == NULL || ctx->error[0] != '\0') {
        return -1;
    }
    if (location.width != 1 && location.width != 2) {
        return set_error(ctx, "scalar access requires byte or word width");
    }
    if (location.storage == SCALAR_LOCAL) {
        if (ctx->stack_depth > 253 ||
            location.address < -(location.width == 1 ? 2 : 1) ||
            location.address > 255 - extra - (int)ctx->stack_depth -
                               (store && location.width == 2 ? 1 : 0)) {
            return set_error(ctx, "local scalar offset exceeds target addressing");
        }
    } else if (location.storage == SCALAR_REGISTER || location.storage == SCALAR_XRAM) {
        int limit = location.storage == SCALAR_REGISTER ? 96 : 65536;
        if (location.address < 0 || location.address > limit - (int)location.width) {
            return set_error(ctx, "scalar address exceeds target memory");
        }
    } else {
        return set_error(ctx, "invalid scalar storage class");
    }
    return 0;
}

int codegen_load_scalar(CodegenContext *ctx, ScalarLocation location)
{
    int address = location.address;
    if (scalar_offset(ctx, location, 0) != 0) {
        return -1;
    }
    if (location.storage == SCALAR_LOCAL) {
        emit_inst(ctx, "LDR");
        emit_number(ctx, "ADIA", address + (int)ctx->stack_depth +
                    (location.width == 1 ? 2 : 1));
        emit_inst(ctx, "STP");
    } else if (location.storage == SCALAR_REGISTER) {
        address += (int)location.width - 1;
        emit_number(ctx, address < 64 ? "LP" : "LIP", address);
    } else {
        emit_number(ctx, "LIDP", address + (int)location.width - 1);
    }
    emit_inst(ctx, location.storage == SCALAR_XRAM ? "LDD" : "LDM");
    if (location.width == 2) {
        emit_inst(ctx, "EXAB");
        if (location.storage == SCALAR_XRAM) {
            if (address / 256 == (address + 1) / 256) {
                emit_number(ctx, "LIDL", address & 255);
            } else {
                emit_number(ctx, "LIDP", address);
            }
        } else {
            emit_inst(ctx, location.storage == SCALAR_LOCAL ? "INCP" : "DECP");
        }
        emit_inst(ctx, location.storage == SCALAR_XRAM ? "LDD" : "LDM");
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

int codegen_store_scalar(CodegenContext *ctx, ScalarLocation location)
{
    if (scalar_offset(ctx, location, 1) != 0) {
        return -1;
    }
    if (location.storage == SCALAR_LOCAL) {
        if (location.width == 1) {
            emit_inst(ctx, "EXAB");
        } else if (codegen_emit_push(ctx, 1) != 0) {
            return -1;
        }
        emit_inst(ctx, "LDR");
        emit_number(ctx, "ADIA", location.address + 2 + (int)ctx->stack_depth);
        emit_inst(ctx, "STP");
        if (location.width == 1) {
            emit_inst(ctx, "EXAB");
        } else if (codegen_emit_pop(ctx, 1) != 0) {
            return -1;
        }
    } else {
        emit_number(ctx, location.storage == SCALAR_XRAM ? "LIDP" :
                    location.address < 64 ? "LP" : "LIP", location.address);
    }
    emit_inst(ctx, location.storage == SCALAR_XRAM ? "STD" : "EXAM");
    if (location.width == 2) {
        emit_inst(ctx, "EXAB");
        if (location.storage == SCALAR_XRAM) {
            if (location.address / 256 == (location.address + 1) / 256) {
                emit_number(ctx, "LIDL", (location.address + 1) & 255);
            } else {
                emit_number(ctx, "LIDP", location.address + 1);
            }
        } else {
            emit_inst(ctx, location.storage == SCALAR_LOCAL ? "DECP" : "INCP");
        }
        emit_inst(ctx, location.storage == SCALAR_XRAM ? "STD" : "EXAM");
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

static void promote_byte(CodegenContext *ctx, int is_unsigned)
{
    char *done;
    emit_inst_op(ctx, "LIB", "0");
    if (is_unsigned) {
        return;
    }
    done = new_label(ctx);
    if (done == NULL) {
        return;
    }
    emit_inst_op(ctx, "TSIA", "128");
    emit_inst_op(ctx, "JPZ", done);
    emit_inst_op(ctx, "LIB", "255");
    emit_label(ctx, done);
    free(done);
}

static int emit_expression(CodegenContext *ctx, const AstExpr *expression);

static int emit_comparison(CodegenContext *ctx, AstBinaryOp op)
{
    const char *routine;
    if (ctx->stack_depth < 2) {
        return set_error(ctx, "evaluation stack underflow");
    }
    switch (op) {
    case AST_BINARY_EQUAL: routine = "LIB_CPE16"; break;
    case AST_BINARY_NOT_EQUAL: routine = "LIB_CPNE16"; break;
    case AST_BINARY_LESS: routine = "LIB_CPS16"; break;
    case AST_BINARY_LESS_EQUAL: routine = "LIB_CPSE16"; break;
    case AST_BINARY_GREATER: routine = "LIB_CPG16"; break;
    case AST_BINARY_GREATER_EQUAL: routine = "LIB_CPGE16"; break;
    default: return set_error(ctx, "invalid comparison operator");
    }
    if (codegen_require_runtime(ctx, routine) != 0) {
        return -1;
    }
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
    emit_inst_op_comment(ctx, "CALL", routine, "Compare unsigned words");
    emit_inst_op(ctx, "LIB", "0");
    return ctx->error[0] == '\0' ? 0 : -1;
}

static int emit_dynamic_array_access(CodegenContext *ctx,
                                     const AstIndexExpr *index,
                                     const AstExpr *value,
                                     int store)
{
    const ScalarSymbol *symbol = symtab_identifier(ctx->symbols, index->array);
    ScalarLocation location;
    int offset;

    if (symbol == NULL || !symbol->is_array) {
        return set_error(ctx, "unresolved dynamic array access");
    }
    location = symbol->location;
    /* Save the RHS across index evaluation, which can itself call functions. */
    if (store && (emit_expression(ctx, value) != 0 ||
                  codegen_emit_push(ctx, 2) != 0)) {
        return -1;
    }
    if (emit_expression(ctx, index->index) != 0) {
        return -1;
    }
    if (location.width == 2) {
        if (codegen_emit_push(ctx, 2) != 0) {
            return -1;
        }
        emit_inst(ctx, "EXAB");
        if (codegen_pop_add(ctx, 2) != 0) {
            return -1;
        }
    }
    if (codegen_emit_push(ctx, 2) != 0) {
        return -1;
    }
    if (location.storage == SCALAR_LOCAL) {
        offset = location.address + (location.width == 1 ? 2 : 1);
        if (offset < 0 || ctx->stack_depth > 255 ||
            offset > 255 - (int)ctx->stack_depth) {
            return set_error(ctx, "local array offset exceeds target addressing");
        }
        emit_inst(ctx, "LDR");
        emit_number(ctx, "ADIA", offset + (int)ctx->stack_depth);
        emit_inst_op(ctx, "LIB", "0");
    } else if (codegen_load_constant(ctx, 2,
                    (unsigned long)location.address) != 0) {
        return -1;
    }
    if (codegen_pop_add(ctx, 2) != 0) {
        return -1;
    }
    if (location.storage == SCALAR_XRAM) {
        emit_number(ctx, "LP", store ? 6 : 4);
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, "EXAB");
        emit_number(ctx, "LP", store ? 7 : 5);
        emit_inst(ctx, "EXAM");
        emit_inst(ctx, store ? "DY" : "DX");
    } else {
        emit_inst(ctx, "STP");
        if (store && location.storage == SCALAR_LOCAL && location.width == 2) {
            emit_inst(ctx, "INCP");
        }
    }
    if (store) {
        if (codegen_emit_pop(ctx, 2) != 0) {
            return -1;
        }
        emit_inst(ctx, location.storage == SCALAR_XRAM ? "IYS" : "EXAM");
        if (location.storage != SCALAR_XRAM) {
            emit_inst(ctx, "LDM");
        }
        if (location.width == 2) {
            emit_inst(ctx, "EXAB");
            if (location.storage != SCALAR_XRAM) {
                emit_inst(ctx, location.storage == SCALAR_LOCAL ? "DECP" : "INCP");
            }
            emit_inst(ctx, location.storage == SCALAR_XRAM ? "IYS" : "EXAM");
            if (location.storage != SCALAR_XRAM) {
                emit_inst(ctx, "LDM");
            }
            emit_inst(ctx, "EXAB");
        }
    } else {
        emit_inst(ctx, location.storage == SCALAR_XRAM ? "IXL" : "LDM");
        if (location.width == 2) {
            emit_inst(ctx, "EXAB");
            if (location.storage != SCALAR_XRAM) {
                emit_inst(ctx, "INCP");
            }
            emit_inst(ctx, location.storage == SCALAR_XRAM ? "IXL" : "LDM");
            if (location.storage != SCALAR_LOCAL) {
                emit_inst(ctx, "EXAB");
            }
        }
    }
    if (location.width == 1) {
        promote_byte(ctx, location.is_unsigned);
    }
    return ctx->error[0] == '\0' ? 0 : -1;
}

static int emit_branch_false(CodegenContext *ctx, const AstExpr *expression,
                             const char *label)
{
    char *low_nonzero;
    if (emit_expression(ctx, expression) != 0) {
        return -1;
    }
    low_nonzero = new_label(ctx);
    if (low_nonzero == NULL) {
        return -1;
    }
    emit_inst_op(ctx, "TSIA", "255");
    emit_inst_op(ctx, "JPNZ", low_nonzero);
    emit_inst(ctx, "EXAB");
    emit_inst_op(ctx, "TSIA", "255");
    emit_inst_op(ctx, "JPZ", label);
    emit_label(ctx, low_nonzero);
    free(low_nonzero);
    return ctx->error[0] == '\0' ? 0 : -1;
}

static int push_loop(CodegenContext *ctx, const char *break_label,
                     const char *continue_label)
{
    if (ctx->loop_count == ctx->loop_capacity) {
        size_t capacity = ctx->loop_capacity == 0 ? 4 : ctx->loop_capacity * 2;
        LoopLabels *loops;
        if (capacity < ctx->loop_capacity ||
            capacity > (size_t)-1 / sizeof(*loops)) {
            return set_error(ctx, "loop nesting exceeds supported range");
        }
        loops = (LoopLabels *)realloc(ctx->loops, capacity * sizeof(*loops));
        if (loops == NULL) {
            return set_error(ctx, "out of memory tracking loop labels");
        }
        ctx->loops = loops;
        ctx->loop_capacity = capacity;
    }
    ctx->loops[ctx->loop_count].break_label = break_label;
    ctx->loops[ctx->loop_count].continue_label = continue_label;
    ++ctx->loop_count;
    return 0;
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
        return codegen_load_constant(ctx, 2, value);
    }
    if (expression->node.kind == AST_NODE_IDENT_EXPR) {
        const ScalarSymbol *symbol = symtab_identifier(ctx->symbols, expression);
        if (symbol == NULL || symbol->is_array) {
            return set_error(ctx, "unresolved scalar reached code generation");
        }
        if (codegen_load_scalar(ctx, symbol->location) != 0) {
            return -1;
        }
        if (symbol->location.width == 1) {
            promote_byte(ctx, symbol->location.is_unsigned);
        }
        return ctx->error[0] == '\0' ? 0 : -1;
    }
    if (expression->node.kind == AST_NODE_INDEX_EXPR) {
        const AstIndexExpr *index = (const AstIndexExpr *)expression;
        ScalarLocation location;
        if (symtab_index_location(ctx->symbols, index, &location) != 0) {
            return emit_dynamic_array_access(ctx, index, NULL, 0);
        }
        if (codegen_load_scalar(ctx, location) != 0) {
            return -1;
        }
        if (location.width == 1) {
            promote_byte(ctx, location.is_unsigned);
        }
        return ctx->error[0] == '\0' ? 0 : -1;
    }
    if (expression->node.kind == AST_NODE_CALL_EXPR) {
        const AstCallExpr *call = (const AstCallExpr *)expression;
        const AstIdentExpr *identifier = (const AstIdentExpr *)call->function;
        const AstFunctionDecl *function =
            symtab_function(ctx->symbols, identifier->name);
        size_t index, argument_bytes = 0, result_width;
        if (function == NULL || function->is_prototype) {
            return set_error(ctx, "unresolved function call reached code generation");
        }
        for (index = 0; index < call->arguments.count; ++index) {
            size_t width = symtab_type_width(function->parameters[index]->type);
            if (emit_expression(ctx, call->arguments.items[index]) != 0 ||
                codegen_emit_push(ctx, width) != 0) {
                return -1;
            }
            if (argument_bytes > (size_t)-1 - width) {
                return set_error(ctx, "function argument stack size overflow");
            }
            argument_bytes += width;
            if (argument_bytes > 255) {
                return set_error(ctx, "function arguments exceed target stack addressing");
            }
        }
        emit_inst_op_comment(ctx, "CALL", function->name, "Function call");
        result_width = symtab_type_width(function->return_type);
        if (argument_bytes != 0) {
            if (ctx->stack_depth < argument_bytes) {
                return set_error(ctx, "function argument stack underflow");
            }
            if (result_width == 2) {
                emit_inst_op(ctx, "LP", "0");
                emit_inst(ctx, "EXAM");
                emit_inst(ctx, "LDR");
                emit_number(ctx, "ADIA", (int)argument_bytes);
                emit_inst(ctx, "STR");
                emit_inst(ctx, "EXAM");
            } else if (result_width == 1) {
                emit_inst(ctx, "EXAB");
                emit_inst(ctx, "LDR");
                emit_number(ctx, "ADIA", (int)argument_bytes);
                emit_inst(ctx, "STR");
                emit_inst(ctx, "EXAB");
            } else if (argument_bytes < 4) {
                for (index = 0; index < argument_bytes; ++index) {
                    emit_inst(ctx, "POP");
                }
            } else {
                emit_inst(ctx, "LDR");
                emit_number(ctx, "ADIA", (int)argument_bytes);
                emit_inst(ctx, "STR");
            }
            ctx->stack_depth -= argument_bytes;
        }
        if (result_width == 1) {
            promote_byte(ctx, function->return_type->as.base.is_unsigned);
        }
        return ctx->error[0] == '\0' ? 0 : -1;
    }
    if (expression->node.kind == AST_NODE_BINARY_EXPR) {
        const AstBinaryExpr *binary = (const AstBinaryExpr *)expression;
        size_t width = 2;

        if (binary->op == AST_BINARY_ASSIGN) {
            ScalarLocation location;
            const ScalarSymbol *symbol = NULL;
            if (binary->left->node.kind == AST_NODE_INDEX_EXPR) {
                const AstIndexExpr *index = (const AstIndexExpr *)binary->left;
                if (symtab_index_location(ctx->symbols, index, &location) != 0) {
                    return emit_dynamic_array_access(ctx, index, binary->right, 1);
                }
            } else {
                symbol = symtab_identifier(ctx->symbols, binary->left);
                if (symbol == NULL || symbol->is_array) {
                    return set_error(ctx, "unresolved scalar assignment reached code generation");
                }
                location = symbol->location;
            }
            if (emit_expression(ctx, binary->right) != 0 ||
                codegen_store_scalar(ctx, location) != 0 ||
                codegen_load_scalar(ctx, location) != 0) {
                return -1;
            }
            if (location.width == 1) {
                promote_byte(ctx, location.is_unsigned);
            }
            return ctx->error[0] == '\0' ? 0 : -1;
        }
        if (binary->op == AST_BINARY_EQUAL ||
            binary->op == AST_BINARY_NOT_EQUAL ||
            binary->op == AST_BINARY_LESS ||
            binary->op == AST_BINARY_LESS_EQUAL ||
            binary->op == AST_BINARY_GREATER ||
            binary->op == AST_BINARY_GREATER_EQUAL) {
            if (emit_expression(ctx, binary->left) != 0 ||
                codegen_emit_push(ctx, width) != 0 ||
                emit_expression(ctx, binary->right) != 0) {
                return -1;
            }
            return emit_comparison(ctx, binary->op);
        }
        if (emit_expression(ctx, binary->left) != 0 ||
            codegen_emit_push(ctx, width) != 0 ||
            emit_expression(ctx, binary->right) != 0) {
            return -1;
        }
        if (binary->op == AST_BINARY_ADD) {
            return codegen_pop_add(ctx, width);
        }
        if (binary->op == AST_BINARY_SUB) {
            return codegen_pop_sub(ctx, width);
        }
        return set_error(ctx, "unsupported binary operator reached code generation");
    }
    return set_error(ctx, "unsupported expression reached during code generation");
}

static int validate_translation_unit(CodegenContext *ctx, AstTranslationUnit *tu)
{
    if (ctx == NULL || tu == NULL) {
        return set_error(ctx, "invalid translation unit");
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    if (ctx->resolved_unit == tu) {
        return 0;
    }
    symtab_destroy(ctx->symbols);
    ctx->symbols = symtab_create();
    if (ctx->symbols == NULL || symtab_resolve(ctx->symbols, tu) != 0) {
        return set_error(ctx, symtab_error(ctx->symbols));
    }
    ctx->resolved_unit = tu;
    return 0;
}

static int emit_statement(CodegenContext *ctx, const AstStmt *statement)
{
    if (statement->node.kind == AST_NODE_COMPOUND_STMT) {
        const AstCompoundStmt *block = (const AstCompoundStmt *)statement;
        size_t index;
        for (index = 0; index < block->item_count; ++index) {
            const AstBlockItem *item = &block->items[index];
            if (item->kind == AST_BLOCK_DECLARATION) {
                const AstVarDecl *variable = (const AstVarDecl *)item->as.declaration;
                const ScalarSymbol *symbol =
                    symtab_declaration(ctx->symbols, variable);
                size_t element;
                if (variable->initializer != NULL && symbol == NULL) {
                    return set_error(ctx, "unresolved local declaration reached code generation");
                }
                if (variable->initializer != NULL && symbol->is_array) {
                    for (element = 0; element < symbol->array_length; ++element) {
                        ScalarLocation location = symbol->location;
                        location.address += (int)(element * location.width);
                        if (element < variable->initializer->as.list.count) {
                            if (emit_expression(ctx, variable->initializer->as.list.items[element]
                                    ->as.expression) != 0) {
                                return -1;
                            }
                        } else if (codegen_load_constant(ctx, location.width, 0) != 0) {
                            return -1;
                        }
                        if (codegen_store_scalar(ctx, location) != 0) {
                            return -1;
                        }
                    }
                } else if (variable->initializer != NULL &&
                    (emit_expression(ctx, variable->initializer->as.expression) != 0 ||
                     codegen_store_scalar(ctx, symbol->location) != 0)) {
                    return -1;
                }
            } else if (emit_statement(ctx, item->as.statement) != 0) {
                return -1;
            }
        }
    } else if (statement->node.kind == AST_NODE_EXPR_STMT) {
        return emit_expression(ctx, ((const AstExprStmt *)statement)->expression);
    } else if (statement->node.kind == AST_NODE_RETURN_STMT) {
        const AstExpr *value = ((const AstReturnStmt *)statement)->expression;
        if (ctx->return_label == NULL ||
            (value != NULL && emit_expression(ctx, value) != 0)) {
            return set_error(ctx, "return statement has no function epilogue");
        }
        if (value != NULL && symtab_type_width(ctx->return_type) == 1) {
            promote_byte(ctx, ctx->return_type->as.base.is_unsigned);
        }
        emit_inst_op(ctx, "JP", ctx->return_label);
    } else if (statement->node.kind == AST_NODE_IF_STMT) {
        const AstIfStmt *conditional = (const AstIfStmt *)statement;
        char *otherwise = new_label(ctx);
        char *done = conditional->else_branch == NULL ? NULL : new_label(ctx);
        if (otherwise == NULL ||
            (conditional->else_branch != NULL && done == NULL)) {
            free(otherwise);
            free(done);
            return -1;
        }
        if (emit_branch_false(ctx, conditional->condition, otherwise) != 0 ||
            emit_statement(ctx, conditional->then_branch) != 0) {
            free(otherwise);
            free(done);
            return -1;
        }
        if (conditional->else_branch != NULL) {
            emit_inst_op(ctx, "JP", done);
        }
        emit_label(ctx, otherwise);
        if (conditional->else_branch != NULL &&
            emit_statement(ctx, conditional->else_branch) != 0) {
            free(otherwise);
            free(done);
            return -1;
        }
        if (done != NULL) {
            emit_label(ctx, done);
        }
        free(otherwise);
        free(done);
    } else if (statement->node.kind == AST_NODE_WHILE_STMT) {
        const AstWhileStmt *loop = (const AstWhileStmt *)statement;
        char *condition = new_label(ctx);
        char *done = new_label(ctx);
        if (condition == NULL || done == NULL) {
            free(condition);
            free(done);
            return -1;
        }
        emit_label(ctx, condition);
        if (emit_branch_false(ctx, loop->condition, done) != 0 ||
            push_loop(ctx, done, condition) != 0) {
            free(condition);
            free(done);
            return -1;
        }
        if (emit_statement(ctx, loop->body) != 0) {
            --ctx->loop_count;
            free(condition);
            free(done);
            return -1;
        }
        --ctx->loop_count;
        emit_inst_op(ctx, "JP", condition);
        emit_label(ctx, done);
        free(condition);
        free(done);
    } else if (statement->node.kind == AST_NODE_DO_WHILE_STMT) {
        const AstDoWhileStmt *loop = (const AstDoWhileStmt *)statement;
        char *body = new_label(ctx);
        char *condition = new_label(ctx);
        char *done = new_label(ctx);
        if (body == NULL || condition == NULL || done == NULL) {
            free(body);
            free(condition);
            free(done);
            return -1;
        }
        emit_label(ctx, body);
        if (push_loop(ctx, done, condition) != 0) {
            free(body);
            free(condition);
            free(done);
            return -1;
        }
        if (emit_statement(ctx, loop->body) != 0) {
            --ctx->loop_count;
            free(body);
            free(condition);
            free(done);
            return -1;
        }
        --ctx->loop_count;
        emit_label(ctx, condition);
        if (emit_branch_false(ctx, loop->condition, done) != 0) {
            free(body);
            free(condition);
            free(done);
            return -1;
        }
        emit_inst_op(ctx, "JP", body);
        emit_label(ctx, done);
        free(body);
        free(condition);
        free(done);
    } else if (statement->node.kind == AST_NODE_FOR_STMT) {
        const AstForStmt *loop = (const AstForStmt *)statement;
        char *condition = new_label(ctx);
        char *step = new_label(ctx);
        char *done = new_label(ctx);
        const ScalarSymbol *symbol = NULL;
        if (condition == NULL || step == NULL || done == NULL) {
            free(condition);
            free(step);
            free(done);
            return -1;
        }
        if (loop->init_declaration != NULL) {
            const AstVarDecl *variable = (const AstVarDecl *)loop->init_declaration;
            symbol = symtab_declaration(ctx->symbols, variable);
            if (symbol == NULL) {
                free(condition);
                free(step);
                free(done);
                return set_error(ctx, "unresolved for-loop declaration");
            }
            if (variable->initializer != NULL &&
                (emit_expression(ctx, variable->initializer->as.expression) != 0 ||
                 codegen_store_scalar(ctx, symbol->location) != 0)) {
                free(condition);
                free(step);
                free(done);
                return -1;
            }
        }
        if (loop->init_expression != NULL &&
            emit_expression(ctx, loop->init_expression) != 0) {
            free(condition);
            free(step);
            free(done);
            return -1;
        }
        emit_label(ctx, condition);
        if ((loop->condition != NULL &&
             emit_branch_false(ctx, loop->condition, done) != 0) ||
            push_loop(ctx, done, step) != 0) {
            free(condition);
            free(step);
            free(done);
            return -1;
        }
        if (emit_statement(ctx, loop->body) != 0) {
            --ctx->loop_count;
            free(condition);
            free(step);
            free(done);
            return -1;
        }
        --ctx->loop_count;
        emit_label(ctx, step);
        if ((loop->step != NULL && emit_expression(ctx, loop->step) != 0) ||
            ctx->error[0] != '\0') {
            free(condition);
            free(step);
            free(done);
            return -1;
        }
        emit_inst_op(ctx, "JP", condition);
        emit_label(ctx, done);
        free(condition);
        free(step);
        free(done);
    } else if (statement->node.kind == AST_NODE_BREAK_STMT ||
               statement->node.kind == AST_NODE_CONTINUE_STMT) {
        const char *label;
        if (ctx->loop_count == 0) {
            return set_error(ctx, "loop transfer has no enclosing loop");
        }
        label = statement->node.kind == AST_NODE_BREAK_STMT
            ? ctx->loops[ctx->loop_count - 1].break_label
            : ctx->loops[ctx->loop_count - 1].continue_label;
        emit_inst_op(ctx, "JP", label);
    }
    return ctx->error[0] == '\0' ? 0 : -1;
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
        size_t frame_size, byte;
        if (function->declaration.node.kind != AST_NODE_FUNCTION_DECL || function->is_prototype) {
            continue;
        }
        if (codegen_stack_depth(ctx) != 0) {
            return set_error(ctx, "evaluation stack is not empty at function entry");
        }
        (void)append_format(ctx, "%s:\t; Procedure\n", function->name, NULL,
                            NULL);
        ctx->return_label = new_label(ctx);
        if (ctx->return_label == NULL) {
            return -1;
        }
        ctx->return_type = function->return_type;
        frame_size = symtab_frame_size(ctx->symbols, function);
        if (frame_size > 255) {
            return set_error(ctx, "function frame exceeds target stack addressing");
        }
        for (byte = 0; byte < frame_size; ++byte) {
            emit_inst(ctx, "PUSH");
            ++ctx->frame_depth;
        }
        if (emit_statement(ctx, (const AstStmt *)function->body) != 0) {
            return -1;
        }
        emit_label(ctx, ctx->return_label);
        if (frame_size != 0) {
            emit_inst_op(ctx, "LP", "0");
            emit_inst(ctx, "EXAM");
            emit_inst(ctx, "EXAB");
            emit_inst_op(ctx, "LP", "1");
            emit_inst(ctx, "EXAM");
            for (byte = 0; byte < frame_size; ++byte) {
                emit_inst(ctx, "POP");
                --ctx->frame_depth;
            }
            emit_inst_op(ctx, "LP", "1");
            emit_inst(ctx, "LDM");
            emit_inst(ctx, "EXAB");
            emit_inst_op(ctx, "LP", "0");
            emit_inst(ctx, "LDM");
        }
        emit_inst(ctx, "RTN");
        (void)append_text(ctx, "\n", 1);
        free(ctx->return_label);
        ctx->return_label = NULL;
        ctx->return_type = NULL;
        if (ctx->error[0] != '\0') {
            return -1;
        }
        if (codegen_stack_depth(ctx) != 0) {
            return set_error(ctx, "evaluation stack is unbalanced at function exit");
        }
    }
    return ctx->error[0] == '\0' ? resolve_runtime(ctx) : -1;
}

static int origin_value(const AstExpr *expression, unsigned long *value)
{
    return symtab_constant(expression, value);
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

static int emit_global_initializers(CodegenContext *ctx,
                                    const AstTranslationUnit *tu)
{
    size_t index;
    for (index = 0; index < tu->declarations.count; ++index) {
        const AstDecl *declaration = tu->declarations.items[index];
        const AstVarDecl *variable;
        const ScalarSymbol *symbol;
        unsigned long value = 0;

        if (declaration->node.kind != AST_NODE_VARIABLE_DECL) {
            continue;
        }
        variable = (const AstVarDecl *)declaration;
        symbol = symtab_declaration(ctx->symbols, variable);
        if (symbol == NULL) {
            return set_error(ctx, "unresolved global reached code generation");
        }
        if (symbol->is_array) {
            size_t element;
            for (element = 0; element < symbol->array_length; ++element) {
                ScalarLocation location = symbol->location;
                location.address += (int)(element * location.width);
                value = 0;
                if (variable->initializer != NULL &&
                    element < variable->initializer->as.list.count &&
                    symtab_initializer_constant(variable->initializer->as.list.items[element]
                        ->as.expression, &value) != 0) {
                    return set_error(ctx,
                        "unsupported global array initializer reached code generation");
                }
                value &= location.width == 1 ? 255UL : 65535UL;
                if (codegen_load_constant(ctx, location.width, value) != 0 ||
                    codegen_store_scalar(ctx, location) != 0) {
                    return -1;
                }
            }
            continue;
        }
        if (variable->initializer != NULL) {
            if (variable->initializer->kind != AST_INITIALIZER_EXPR ||
                symtab_initializer_constant(variable->initializer->as.expression,
                                            &value) != 0) {
                return set_error(ctx,
                    "unsupported global initializer reached code generation");
            }
        }
        value &= symbol->location.width == 1 ? 255UL : 65535UL;
        if (codegen_load_constant(ctx, symbol->location.width, value) != 0 ||
            codegen_store_scalar(ctx, symbol->location) != 0) {
            return -1;
        }
    }
    return ctx->error[0] == '\0' ? 0 : -1;
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
    if (ctx->output_length != 0 || codegen_stack_depth(ctx) != 0 ||
        ctx->startup_output != NULL || ctx->function_output != NULL) {
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
        if (function->declaration.node.kind != AST_NODE_FUNCTION_DECL) {
            continue;
        }
        if (toupper((unsigned char)function->name[0]) == 'L' &&
            toupper((unsigned char)function->name[1]) == 'B' &&
            isdigit((unsigned char)function->name[2])) {
            return set_error(ctx, "LB followed by digits is reserved for compiler labels");
        }
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
            if (previous->declaration.node.kind == AST_NODE_FUNCTION_DECL && !previous->is_prototype &&
                same_assembler_name(function->name, previous->name)) {
                return set_error(ctx, "function definitions collide in PASM's case-insensitive namespace");
            }
        }
    }
    if (!has_main) {
        return set_error(ctx, "program requires a parameterless main definition");
    }
    for (index = 0; index < tu->declarations.count; ++index) {
        const AstFunctionDecl *function =
            (const AstFunctionDecl *)tu->declarations.items[index];
        if (function->declaration.node.kind == AST_NODE_FUNCTION_DECL &&
            !function->is_prototype && strcmp(function->name, "main") == 0 &&
            function->parameter_count != 0) {
            return set_error(ctx, "program main definition must be parameterless");
        }
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
    if (emit_global_initializers(ctx, tu) != 0) {
        return -1;
    }
    emit_inst_op(ctx, "CALL", "main");
    if (!tu->no_save) {
        emit_inst_op(ctx, "LP", "0");
        emit_inst_op(ctx, "LIDP", "SREG");
        emit_inst_op(ctx, "LII", "11");
        emit_inst(ctx, "MVWD");
    }
    emit_inst(ctx, "RTN");
    if (ctx->error[0] != '\0') {
        return -1;
    }
    ctx->startup_output = take_output(ctx);
    if (!tu->no_save) {
        (void)append_text(ctx, "SREG:\t.DW 0, 0, 0, 0, 0, 0\n\n",
                          strlen("SREG:\t.DW 0, 0, 0, 0, 0, 0\n\n"));
    }
    if (ctx->error[0] != '\0') {
        return -1;
    }
    ctx->data_output = take_output(ctx);
    if (codegen_translation_unit(ctx, tu) != 0) {
        return -1;
    }
    ctx->function_output = take_output(ctx);
    for (index = 0; index < tu->declarations.count; ++index) {
        const AstFunctionDecl *function =
            (const AstFunctionDecl *)tu->declarations.items[index];
        size_t routine, label;
        if (function->declaration.node.kind != AST_NODE_FUNCTION_DECL ||
            function->is_prototype) {
            continue;
        }
        for (routine = 0; routine < RUNTIME_COUNT; ++routine) {
            if (!ctx->library_used[routine]) {
                continue;
            }
            for (label = 0; label < 3; ++label) {
                const char *name = runtime_routines[routine].labels[label];
                if (name != NULL && same_assembler_name(function->name, name)) {
                    return set_error(ctx,
                        "function definition collides with a runtime library label");
                }
            }
        }
    }
    for (index = CODEGEN_STARTUP; index <= CODEGEN_RUNTIME; ++index) {
        const char *section = codegen_section_output(ctx, (CodegenSection)index);
        if (append_text(ctx, section, strlen(section)) != 0) {
            return -1;
        }
    }
    return 0;
}
