#ifndef LCC2_CODEGEN_H
#define LCC2_CODEGEN_H

#include "ast.h"
#include "symtab.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CodegenContext CodegenContext;

typedef enum CodegenSection {
    CODEGEN_STARTUP,
    CODEGEN_FUNCTIONS,
    CODEGEN_DATA,
    CODEGEN_RUNTIME
} CodegenSection;

CodegenContext *codegen_context_create(void);
void codegen_context_destroy(CodegenContext *ctx);
const char *codegen_output(const CodegenContext *ctx);
const char *codegen_runtime_output(const CodegenContext *ctx);
const char *codegen_section_output(const CodegenContext *ctx,
                                   CodegenSection section);
int codegen_require_runtime(CodegenContext *ctx, const char *routine);
const char *codegen_error(const CodegenContext *ctx);
size_t codegen_stack_depth(const CodegenContext *ctx);

void emit_inst(CodegenContext *ctx, const char *inst);
void emit_inst_op(CodegenContext *ctx, const char *inst, const char *operand);
void emit_inst_op_comment(CodegenContext *ctx, const char *inst,
                          const char *operand, const char *comment);
void emit_comment(CodegenContext *ctx, const char *comment);
void emit_label(CodegenContext *ctx, const char *label);
char *new_label(CodegenContext *ctx);

int codegen_emit_push(CodegenContext *ctx, size_t width);
int codegen_emit_pop(CodegenContext *ctx, size_t width);
int codegen_pop_add(CodegenContext *ctx, size_t width);
int codegen_pop_sub(CodegenContext *ctx, size_t width);
int codegen_load_constant(CodegenContext *ctx, size_t width,
                          unsigned long value);
int codegen_load_scalar(CodegenContext *ctx, ScalarLocation location);
int codegen_store_scalar(CodegenContext *ctx, ScalarLocation location);

int codegen_translation_unit(CodegenContext *ctx, AstTranslationUnit *tu);
int codegen_program(CodegenContext *ctx, AstTranslationUnit *tu);

#ifdef __cplusplus
}
#endif

#endif
