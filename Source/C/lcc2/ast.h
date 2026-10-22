#ifndef LCC2_AST_H
#define LCC2_AST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AstArena AstArena;

/* Allocations are zero-initialized and live until the arena is destroyed. */
AstArena *ast_arena_create(void);
void ast_arena_destroy(AstArena *arena);
void *ast_arena_alloc(AstArena *arena, size_t size);
char *ast_arena_strdup(AstArena *arena, const char *text);
char *ast_arena_strndup(AstArena *arena, const char *text, size_t length);

typedef struct AstNode AstNode;
typedef struct AstExpr AstExpr;
typedef struct AstStmt AstStmt;
typedef struct AstDecl AstDecl;
typedef struct AstType AstType;
typedef struct AstInitializer AstInitializer;
typedef struct AstTranslationUnit AstTranslationUnit;
typedef struct AstVarDecl AstVarDecl;
typedef struct AstParamDecl AstParamDecl;
typedef struct AstFunctionDecl AstFunctionDecl;
typedef struct AstStructDecl AstStructDecl;
typedef struct AstUnionDecl AstUnionDecl;
typedef struct AstEnumDecl AstEnumDecl;
typedef struct AstEnumerator AstEnumerator;
typedef struct AstTypedefDecl AstTypedefDecl;
typedef struct AstCompoundStmt AstCompoundStmt;
typedef struct AstBlockItem AstBlockItem;

typedef struct AstSourceLocation {
    const char *file;
    unsigned long line;
    unsigned long column;
    size_t offset;
} AstSourceLocation;

typedef struct AstSourceSpan {
    AstSourceLocation start;
    AstSourceLocation end;
} AstSourceSpan;

typedef enum AstNodeKind {
    AST_NODE_TRANSLATION_UNIT,
    AST_NODE_VARIABLE_DECL,
    AST_NODE_PARAMETER_DECL,
    AST_NODE_FUNCTION_DECL,
    AST_NODE_STRUCT_DECL,
    AST_NODE_UNION_DECL,
    AST_NODE_ENUM_DECL,
    AST_NODE_ENUMERATOR,
    AST_NODE_TYPEDEF_DECL,
    AST_NODE_COMPOUND_STMT,
    AST_NODE_IF_STMT,
    AST_NODE_WHILE_STMT,
    AST_NODE_DO_WHILE_STMT,
    AST_NODE_FOR_STMT,
    AST_NODE_SWITCH_STMT,
    AST_NODE_CASE_STMT,
    AST_NODE_DEFAULT_STMT,
    AST_NODE_RETURN_STMT,
    AST_NODE_BREAK_STMT,
    AST_NODE_CONTINUE_STMT,
    AST_NODE_GOTO_STMT,
    AST_NODE_LABEL_STMT,
    AST_NODE_EXPR_STMT,
    AST_NODE_EMPTY_STMT,
    AST_NODE_ASM_STMT,
    AST_NODE_BINARY_EXPR,
    AST_NODE_UNARY_EXPR,
    AST_NODE_TERNARY_EXPR,
    AST_NODE_CAST_EXPR,
    AST_NODE_CALL_EXPR,
    AST_NODE_INDEX_EXPR,
    AST_NODE_MEMBER_EXPR,
    AST_NODE_IDENT_EXPR,
    AST_NODE_CONSTANT_EXPR,
    AST_NODE_STRING_EXPR,
    AST_NODE_SIZEOF_EXPR,
    AST_NODE_ADDRESS_EXPR,
    AST_NODE_DEREFERENCE_EXPR
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    AstSourceSpan span;
};

typedef enum AstBaseType {
    AST_TYPE_VOID,
    AST_TYPE_CHAR,
    AST_TYPE_INT,
    AST_TYPE_FLOAT
} AstBaseType;

typedef enum AstTypeKind {
    AST_TYPE_BASE,
    AST_TYPE_POINTER,
    AST_TYPE_ARRAY,
    AST_TYPE_STRUCT,
    AST_TYPE_UNION,
    AST_TYPE_ENUM
} AstTypeKind;

enum {
    AST_QUALIFIER_CONST = 1u << 0,
    AST_QUALIFIER_VOLATILE = 1u << 1
};

struct AstType {
    AstTypeKind kind;
    unsigned int qualifiers;
    AstSourceSpan span;
    union {
        struct {
            AstBaseType base;
            unsigned char is_unsigned;
        } base;
        struct {
            AstType *target;
        } pointer;
        struct {
            AstType *element;
            AstExpr *length;
            size_t resolved_length;
            unsigned char has_resolved_length;
        } array;
        struct {
            AstStructDecl *declaration;
        } structure;
        struct {
            AstUnionDecl *declaration;
        } union_type;
        struct {
            AstEnumDecl *declaration;
        } enumeration;
    } as;
};

typedef enum AstStorageClass {
    AST_STORAGE_NONE,
    AST_STORAGE_STATIC,
    AST_STORAGE_EXTERN
} AstStorageClass;

typedef struct AstAttributes {
    unsigned char has_address;
    AstExpr *address;
    unsigned char is_xram;
} AstAttributes;

typedef struct AstExprList {
    AstExpr **items;
    size_t count;
} AstExprList;

typedef struct AstDeclList {
    AstDecl **items;
    size_t count;
} AstDeclList;

struct AstTranslationUnit {
    AstNode node;
    AstDeclList declarations;
};

struct AstDecl {
    AstNode node;
};

struct AstVarDecl {
    AstDecl declaration;
    AstType *type;
    const char *name;
    AstInitializer *initializer;
    AstAttributes attributes;
    AstStorageClass storage;
};

struct AstParamDecl {
    AstDecl declaration;
    AstType *type;
    const char *name;
};

struct AstFunctionDecl {
    AstDecl declaration;
    AstType *return_type;
    const char *name;
    AstParamDecl **parameters;
    size_t parameter_count;
    AstCompoundStmt *body;
    unsigned char is_prototype;
    AstStorageClass storage;
};

typedef struct AstStructMember {
    AstType *type;
    AstVarDecl **declarators;
    size_t declarator_count;
    AstSourceSpan span;
} AstStructMember;

struct AstStructDecl {
    AstDecl declaration;
    const char *tag;
    AstStructMember *members;
    size_t member_count;
    unsigned char is_complete;
};

struct AstUnionDecl {
    AstDecl declaration;
    const char *tag;
    AstStructMember *members;
    size_t member_count;
    unsigned char is_complete;
};

struct AstEnumDecl {
    AstDecl declaration;
    const char *tag;
    AstEnumerator **enumerators;
    size_t enumerator_count;
    unsigned char is_complete;
};

struct AstEnumerator {
    AstNode node;
    const char *name;
    AstExpr *value;
};

struct AstTypedefDecl {
    AstDecl declaration;
    AstType *type;
    const char *alias;
};

typedef enum AstInitializerKind {
    AST_INITIALIZER_EXPR,
    AST_INITIALIZER_LIST
} AstInitializerKind;

struct AstInitializer {
    AstInitializerKind kind;
    AstSourceSpan span;
    union {
        AstExpr *expression;
        struct {
            AstInitializer **items;
            size_t count;
        } list;
    } as;
};

struct AstExpr {
    AstNode node;
};

struct AstStmt {
    AstNode node;
};

typedef enum AstBlockItemKind {
    AST_BLOCK_DECLARATION,
    AST_BLOCK_STATEMENT
} AstBlockItemKind;

struct AstBlockItem {
    AstBlockItemKind kind;
    union {
        AstDecl *declaration;
        AstStmt *statement;
    } as;
};

struct AstCompoundStmt {
    AstStmt statement;
    AstBlockItem *items;
    size_t item_count;
};

typedef struct AstIfStmt {
    AstStmt statement;
    AstExpr *condition;
    AstStmt *then_branch;
    AstStmt *else_branch;
} AstIfStmt;

typedef struct AstWhileStmt {
    AstStmt statement;
    AstExpr *condition;
    AstStmt *body;
} AstWhileStmt;

typedef struct AstDoWhileStmt {
    AstStmt statement;
    AstStmt *body;
    AstExpr *condition;
} AstDoWhileStmt;

typedef struct AstForStmt {
    AstStmt statement;
    AstDecl *init_declaration;
    AstExpr *init_expression;
    AstExpr *condition;
    AstExpr *step;
    AstStmt *body;
} AstForStmt;

typedef struct AstSwitchStmt {
    AstStmt statement;
    AstExpr *expression;
    AstStmt *body;
} AstSwitchStmt;

typedef struct AstCaseStmt {
    AstStmt statement;
    AstExpr *value;
    AstStmt *body;
} AstCaseStmt;

typedef struct AstDefaultStmt {
    AstStmt statement;
    AstStmt *body;
} AstDefaultStmt;

typedef struct AstReturnStmt {
    AstStmt statement;
    AstExpr *expression;
} AstReturnStmt;

typedef struct AstBreakStmt {
    AstStmt statement;
} AstBreakStmt;

typedef struct AstContinueStmt {
    AstStmt statement;
} AstContinueStmt;

typedef struct AstGotoStmt {
    AstStmt statement;
    const char *label;
} AstGotoStmt;

typedef struct AstLabelStmt {
    AstStmt statement;
    const char *name;
    AstStmt *body;
} AstLabelStmt;

typedef struct AstExprStmt {
    AstStmt statement;
    AstExpr *expression;
} AstExprStmt;

typedef struct AstEmptyStmt {
    AstStmt statement;
} AstEmptyStmt;

typedef struct AstAsmStmt {
    AstStmt statement;
    const char *code;
    size_t code_length;
} AstAsmStmt;

typedef enum AstBinaryOp {
    AST_BINARY_MUL,
    AST_BINARY_DIV,
    AST_BINARY_MOD,
    AST_BINARY_ADD,
    AST_BINARY_SUB,
    AST_BINARY_SHIFT_LEFT,
    AST_BINARY_SHIFT_RIGHT,
    AST_BINARY_LESS,
    AST_BINARY_LESS_EQUAL,
    AST_BINARY_GREATER,
    AST_BINARY_GREATER_EQUAL,
    AST_BINARY_EQUAL,
    AST_BINARY_NOT_EQUAL,
    AST_BINARY_BIT_AND,
    AST_BINARY_BIT_XOR,
    AST_BINARY_BIT_OR,
    AST_BINARY_LOGICAL_AND,
    AST_BINARY_LOGICAL_OR,
    AST_BINARY_ASSIGN,
    AST_BINARY_MUL_ASSIGN,
    AST_BINARY_DIV_ASSIGN,
    AST_BINARY_MOD_ASSIGN,
    AST_BINARY_ADD_ASSIGN,
    AST_BINARY_SUB_ASSIGN,
    AST_BINARY_SHIFT_LEFT_ASSIGN,
    AST_BINARY_SHIFT_RIGHT_ASSIGN,
    AST_BINARY_AND_ASSIGN,
    AST_BINARY_XOR_ASSIGN,
    AST_BINARY_OR_ASSIGN,
    AST_BINARY_COMMA
} AstBinaryOp;

typedef enum AstUnaryOp {
    AST_UNARY_PLUS,
    AST_UNARY_MINUS,
    AST_UNARY_LOGICAL_NOT,
    AST_UNARY_BITWISE_NOT,
    AST_UNARY_PRE_INCREMENT,
    AST_UNARY_PRE_DECREMENT,
    AST_UNARY_POST_INCREMENT,
    AST_UNARY_POST_DECREMENT
} AstUnaryOp;

typedef struct AstBinaryExpr {
    AstExpr expression;
    AstBinaryOp op;
    AstExpr *left;
    AstExpr *right;
} AstBinaryExpr;

typedef struct AstUnaryExpr {
    AstExpr expression;
    AstUnaryOp op;
    AstExpr *operand;
} AstUnaryExpr;

typedef struct AstTernaryExpr {
    AstExpr expression;
    AstExpr *condition;
    AstExpr *then_expression;
    AstExpr *else_expression;
} AstTernaryExpr;

typedef struct AstCastExpr {
    AstExpr expression;
    AstType *type;
    AstExpr *operand;
} AstCastExpr;

typedef struct AstCallExpr {
    AstExpr expression;
    AstExpr *function;
    AstExprList arguments;
} AstCallExpr;

typedef struct AstIndexExpr {
    AstExpr expression;
    AstExpr *array;
    AstExpr *index;
} AstIndexExpr;

typedef struct AstMemberExpr {
    AstExpr expression;
    AstExpr *object;
    const char *field;
    unsigned char is_arrow;
} AstMemberExpr;

typedef struct AstIdentExpr {
    AstExpr expression;
    const char *name;
} AstIdentExpr;

typedef enum AstConstantKind {
    AST_CONSTANT_INTEGER,
    AST_CONSTANT_FLOAT,
    AST_CONSTANT_CHARACTER
} AstConstantKind;

typedef struct AstConstantExpr {
    AstExpr expression;
    AstConstantKind constant_kind;
    AstType *type;
    const char *spelling;
    union {
        long integer;
        double floating;
        unsigned long character;
    } value;
} AstConstantExpr;

typedef struct AstStringExpr {
    AstExpr expression;
    const unsigned char *bytes;
    size_t length;
} AstStringExpr;

typedef struct AstSizeofExpr {
    AstExpr expression;
    unsigned char is_type_operand;
    union {
        AstType *type;
        AstExpr *expression;
    } operand;
} AstSizeofExpr;

typedef struct AstAddressExpr {
    AstExpr expression;
    AstExpr *operand;
} AstAddressExpr;

typedef struct AstDereferenceExpr {
    AstExpr expression;
    AstExpr *operand;
} AstDereferenceExpr;

#ifdef __cplusplus
}
#endif

#endif
