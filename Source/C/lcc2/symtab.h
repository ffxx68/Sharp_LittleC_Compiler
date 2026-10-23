#ifndef LCC2_SYMTAB_H
#define LCC2_SYMTAB_H

#include "ast.h"

typedef enum ScalarStorage {
    SCALAR_REGISTER,
    SCALAR_LOCAL,
    SCALAR_XRAM
} ScalarStorage;

typedef struct ScalarLocation {
    ScalarStorage storage;
    size_t width;
    int address; /* Absolute address, or legacy local offset relative to the frame. */
    unsigned char is_unsigned;
} ScalarLocation;

typedef struct ScalarSymbol {
    const AstVarDecl *declaration;
    const AstParamDecl *parameter;
    const AstFunctionDecl *function;
    ScalarLocation location;
    size_t allocation_width;
    size_t array_length;
    size_t scope;
    unsigned char is_array;
    unsigned char active;
} ScalarSymbol;

typedef struct ScalarSymbols ScalarSymbols;

ScalarSymbols *symtab_create(void);
void symtab_destroy(ScalarSymbols *table);
int symtab_resolve(ScalarSymbols *table, AstTranslationUnit *unit);
const char *symtab_error(const ScalarSymbols *table);
const ScalarSymbol *symtab_identifier(const ScalarSymbols *table,
                                      const AstExpr *expression);
int symtab_index_location(const ScalarSymbols *table,
                          const AstIndexExpr *expression,
                          ScalarLocation *location);
const ScalarSymbol *symtab_declaration(const ScalarSymbols *table,
                                       const AstVarDecl *declaration);
const ScalarSymbol *symtab_parameter(const ScalarSymbols *table,
                                     const AstParamDecl *parameter);
const AstFunctionDecl *symtab_function(const ScalarSymbols *table,
                                      const char *name);
size_t symtab_frame_size(const ScalarSymbols *table,
                         const AstFunctionDecl *function);
size_t symtab_type_width(const AstType *type);
int symtab_constant(const AstExpr *expression, unsigned long *value);
int symtab_initializer_constant(const AstExpr *expression,
                                unsigned long *value);

#endif
