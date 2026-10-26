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

typedef struct ResolvedAddress {
    const AstType *pointee_type;
    ScalarLocation location;
    size_t pointer_width;
} ResolvedAddress;

typedef struct ResolvedDereference {
    const AstType *pointee_type;
    size_t width;
    unsigned char is_unsigned;
} ResolvedDereference;

typedef struct ResolvedStructMember {
    const AstStructDecl *structure;
    const AstVarDecl *declaration;
    const char *name;
    const AstType *type;
    size_t offset; /* Byte offset in the tightly packed structure. */
    size_t width;
} ResolvedStructMember;

typedef struct ResolvedMemberAccess {
    ResolvedStructMember member;
    ScalarLocation location;
    const AstExpr *pointer_expression;
    unsigned char is_modifiable;
    unsigned char is_indirect;
} ResolvedMemberAccess;

typedef struct ScalarSymbol {
    const AstVarDecl *declaration;
    const AstParamDecl *parameter;
    const AstFunctionDecl *function;
    ScalarLocation location;
    size_t allocation_width;
    size_t array_length;
    size_t scope;
    unsigned char is_array;
    unsigned char is_aggregate;
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
int symtab_address_info(const ScalarSymbols *table,
                        const AstAddressExpr *expression,
                        ResolvedAddress *address);
int symtab_dereference_info(const ScalarSymbols *table,
                            const AstDereferenceExpr *expression,
                            ResolvedDereference *dereference);
const ScalarSymbol *symtab_declaration(const ScalarSymbols *table,
                                       const AstVarDecl *declaration);
const ScalarSymbol *symtab_parameter(const ScalarSymbols *table,
                                     const AstParamDecl *parameter);
const AstFunctionDecl *symtab_function(const ScalarSymbols *table,
                                      const char *name);
size_t symtab_frame_size(const ScalarSymbols *table,
                         const AstFunctionDecl *function);
size_t symtab_type_width(const AstType *type);
int symtab_structure_size(const ScalarSymbols *table,
                          const AstStructDecl *structure, size_t *size);
int symtab_structure_member(const ScalarSymbols *table,
                            const AstStructDecl *structure, const char *name,
                            ResolvedStructMember *member);
int symtab_member_access(const ScalarSymbols *table,
                         const AstMemberExpr *expression,
                         ResolvedMemberAccess *access);
/* Compound assignment and ++/-- are rewritten as "lhs = lhs op rhs"; the
 * lvalue is guaranteed side-effect free, so evaluating it twice is exact. */
typedef struct SymtabUpdateRewrite {
    AstBinaryExpr assign;
    AstBinaryExpr operation;
    AstBinaryExpr scale;
    AstConstantExpr step;
    AstConstantExpr factor;
    const AstExpr *lvalue;
    int postfix;
    int is_pointer;
} SymtabUpdateRewrite;

/* Returns 1 and fills the rewrite for an update expression, 0 if the
 * expression is not one, or -1 if a pointer target size is unknown. */
int symtab_update_rewrite(const ScalarSymbols *table,
                          const AstExpr *expression,
                          SymtabUpdateRewrite *rewrite);
int symtab_constant(const AstExpr *expression, unsigned long *value);
int symtab_initializer_constant(const AstExpr *expression,
                                unsigned long *value);
int symtab_switch_width(const ScalarSymbols *table, const AstExpr *expression,
                        size_t *width);

#endif
