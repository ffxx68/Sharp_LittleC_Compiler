%code requires {
#include "ast.h"
typedef struct ParseList ParseList;
typedef struct ParseSpec ParseSpec;
typedef struct ParseDeclarator ParseDeclarator;
typedef struct ParseTypedef ParseTypedef;
}

%{
#include "parser.h"
#include "lexer_tokens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ParseList ParseList;
typedef struct ParseSpec ParseSpec;
typedef struct ParseDeclarator ParseDeclarator;
typedef struct ParseTypedef ParseTypedef;

struct ParseList {
    void **items;
    size_t count;
    size_t capacity;
    struct ParseList *next;
};

struct ParseSpec {
    AstType *type;
    AstStorageClass storage;
    unsigned char is_typedef;
    AstAttributes attributes;
};

struct ParseDeclarator {
    const char *name;
    AstType *type;
    AstParamDecl **parameters;
    size_t parameter_count;
    size_t pointer_count;
    unsigned char is_function;
    AstInitializer *initializer;
};

struct ParseTypedef {
    const char *name;
    AstType *type;
};

static AstArena *parse_arena;
static AstTranslationUnit *parsed_unit;
static ParseList *parsed_declarations;
static ParseList *parse_lists;
static ParseList *typedef_names;
static size_t *typedef_scope_marks;
static size_t typedef_scope_count;
static size_t typedef_scope_capacity;

int yylex(void);
void yyerror(const char *message);

static ParseList *list_new(void)
{
    ParseList *list = (ParseList *)calloc(1, sizeof(*list));
    if (list == NULL) {
        return NULL;
    }
    list->next = parse_lists;
    parse_lists = list;
    return list;
}

static ParseList *list_append(ParseList *list, void *item)
{
    void **items;
    size_t capacity;

    if (list == NULL) {
        list = list_new();
    }
    if (list == NULL) {
        return NULL;
    }
    if (list->count == list->capacity) {
        capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        if (capacity < list->capacity ||
            capacity > (size_t)-1 / sizeof(*items)) {
            return NULL;
        }
        items = (void **)realloc(list->items, capacity * sizeof(*items));
        if (items == NULL) {
            return NULL;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = item;
    return list;
}

static void free_parse_lists(void)
{
    while (parse_lists != NULL) {
        ParseList *next = parse_lists->next;
        free(parse_lists->items);
        free(parse_lists);
        parse_lists = next;
    }
}

static const char *copy_text(const char *text, size_t length)
{
    return ast_arena_strndup(parse_arena, text, length);
}

static AstType *base_type(AstBaseType base, int is_unsigned)
{
    AstType *type = (AstType *)ast_arena_alloc(parse_arena, sizeof(*type));
    if (type != NULL) {
        type->kind = AST_TYPE_BASE;
        type->as.base.base = base;
        type->as.base.is_unsigned = (unsigned char)(is_unsigned != 0);
    }
    return type;
}

static AstType *pointer_type(AstType *target)
{
    AstType *type = (AstType *)ast_arena_alloc(parse_arena, sizeof(*type));
    if (type != NULL) {
        type->kind = AST_TYPE_POINTER;
        type->as.pointer.target = target;
    }
    return type;
}

static AstType *complete_type(AstType *type, AstType *base,
                              size_t pointer_count)
{
    if (type == NULL) {
        while (pointer_count > 0) {
            base = pointer_type(base);
            if (base == NULL) {
                return NULL;
            }
            --pointer_count;
        }
        return base;
    }
    if (type->kind == AST_TYPE_POINTER) {
        type->as.pointer.target = complete_type(type->as.pointer.target, base,
                                                pointer_count);
    } else if (type->kind == AST_TYPE_ARRAY) {
        type->as.array.element = complete_type(type->as.array.element, base,
                                               pointer_count);
    }
    return type;
}

static ParseTypedef *find_typedef(const char *name, size_t length)
{
    size_t index;

    if (typedef_names == NULL) {
        return NULL;
    }
    index = typedef_names->count;
    while (index != 0) {
        ParseTypedef *entry = (ParseTypedef *)typedef_names->items[--index];
        if (strlen(entry->name) == length &&
            memcmp(entry->name, name, length) == 0) {
            return entry;
        }
    }
    return NULL;
}

int lcc2_parser_is_typedef_name(const char *name, size_t length)
{
    return find_typedef(name, length) != NULL;
}

static AstType *typedef_type(const char *name)
{
    ParseTypedef *entry = find_typedef(name, strlen(name));
    return entry == NULL ? NULL : entry->type;
}

static int register_typedef(ParseDeclarator *declarator, AstType *type)
{
    ParseTypedef *entry = (ParseTypedef *)ast_arena_alloc(parse_arena,
                                                          sizeof(*entry));
    if (entry == NULL) {
        return 0;
    }
    entry->name = declarator->name;
    entry->type = complete_type(declarator->type, type,
                                declarator->pointer_count);
    return entry->type != NULL && list_append(typedef_names, entry) != NULL;
}

static int push_typedef_scope(void)
{
    size_t *marks;
    size_t capacity;

    if (typedef_scope_count == typedef_scope_capacity) {
        capacity = typedef_scope_capacity == 0 ? 4 : typedef_scope_capacity * 2;
        if (capacity < typedef_scope_capacity ||
            capacity > (size_t)-1 / sizeof(*marks)) {
            return 0;
        }
        marks = (size_t *)realloc(typedef_scope_marks,
                                  capacity * sizeof(*marks));
        if (marks == NULL) {
            return 0;
        }
        typedef_scope_marks = marks;
        typedef_scope_capacity = capacity;
    }
    typedef_scope_marks[typedef_scope_count++] = typedef_names->count;
    return 1;
}

static void pop_typedef_scope(void)
{
    if (typedef_scope_count != 0) {
        typedef_names->count = typedef_scope_marks[--typedef_scope_count];
    }
}

static AstExpr *new_expr(AstNodeKind kind, size_t size)
{
    AstNode *node = (AstNode *)ast_arena_alloc(parse_arena, size);
    if (node != NULL) {
        node->kind = kind;
        node->span.start.line = lcc2_token_line;
        node->span.start.column = lcc2_token_column;
        node->span.end = node->span.start;
    }
    return (AstExpr *)node;
}

static AstStmt *new_stmt(AstNodeKind kind, size_t size)
{
    AstNode *node = (AstNode *)ast_arena_alloc(parse_arena, size);
    if (node != NULL) {
        node->kind = kind;
        node->span.start.line = lcc2_token_line;
        node->span.start.column = lcc2_token_column;
        node->span.end = node->span.start;
    }
    return (AstStmt *)node;
}

static AstExpr *binary_expr(AstBinaryOp op, AstExpr *left, AstExpr *right)
{
    AstBinaryExpr *expression =
        (AstBinaryExpr *)new_expr(AST_NODE_BINARY_EXPR, sizeof(*expression));
    if (expression != NULL) {
        expression->op = op;
        expression->left = left;
        expression->right = right;
    }
    return (AstExpr *)expression;
}

static AstExpr *unary_expr(AstUnaryOp op, AstExpr *operand)
{
    AstUnaryExpr *expression =
        (AstUnaryExpr *)new_expr(AST_NODE_UNARY_EXPR, sizeof(*expression));
    if (expression != NULL) {
        expression->op = op;
        expression->operand = operand;
    }
    return (AstExpr *)expression;
}

static AstDecl *variable_decl(AstType *type, const char *name,
                              AstInitializer *initializer,
                              AstStorageClass storage)
{
    AstVarDecl *declaration =
        (AstVarDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
    if (declaration != NULL) {
        declaration->declaration.node.kind = AST_NODE_VARIABLE_DECL;
        declaration->type = type;
        declaration->name = name;
        declaration->initializer = initializer;
        declaration->storage = storage;
    }
    return (AstDecl *)declaration;
}

static AstDecl *function_decl(ParseDeclarator *declarator, AstType *return_type,
                             AstCompoundStmt *body,
                             AstStorageClass storage)
{
    AstFunctionDecl *declaration =
        (AstFunctionDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
    if (declaration != NULL) {
        if (declarator->parameter_count == 1 &&
            declarator->parameters[0]->name == NULL &&
            declarator->parameters[0]->type->kind == AST_TYPE_BASE &&
            declarator->parameters[0]->type->as.base.base == AST_TYPE_VOID) {
            declarator->parameters = NULL;
            declarator->parameter_count = 0;
        }
        declaration->declaration.node.kind = AST_NODE_FUNCTION_DECL;
        declaration->return_type = return_type;
        declaration->name = declarator->name;
        declaration->parameters = declarator->parameters;
        declaration->parameter_count = declarator->parameter_count;
        declaration->body = body;
        declaration->is_prototype = (unsigned char)(body == NULL);
        declaration->storage = storage;
    }
    return (AstDecl *)declaration;
}

static AstDecl *make_parameter(ParseDeclarator *declarator, AstType *type)
{
    AstParamDecl *parameter =
        (AstParamDecl *)ast_arena_alloc(parse_arena, sizeof(*parameter));
    if (parameter != NULL) {
        parameter->declaration.node.kind = AST_NODE_PARAMETER_DECL;
        parameter->type = declarator == NULL ? type : declarator->type;
        parameter->name = declarator == NULL ? NULL : declarator->name;
    }
    return (AstDecl *)parameter;
}

static AstInitializer *expression_initializer(AstExpr *expression)
{
    AstInitializer *initializer =
        (AstInitializer *)ast_arena_alloc(parse_arena, sizeof(*initializer));
    if (initializer != NULL) {
        initializer->kind = AST_INITIALIZER_EXPR;
        initializer->as.expression = expression;
    }
    return initializer;
}

static AstInitializer *list_initializer(ParseList *items)
{
    AstInitializer *initializer =
        (AstInitializer *)ast_arena_alloc(parse_arena, sizeof(*initializer));
    size_t index;

    if (initializer == NULL) {
        return NULL;
    }
    initializer->kind = AST_INITIALIZER_LIST;
    initializer->as.list.count = items == NULL ? 0 : items->count;
    initializer->as.list.items = (AstInitializer **)ast_arena_alloc(parse_arena,
        initializer->as.list.count * sizeof(*initializer->as.list.items));
    for (index = 0; initializer->as.list.items != NULL &&
         index < initializer->as.list.count; ++index) {
        initializer->as.list.items[index] =
            (AstInitializer *)items->items[index];
    }
    return initializer;
}

static ParseList *make_struct_member(ParseSpec *spec, ParseList *declarators)
{
    AstStructMember *member =
        (AstStructMember *)ast_arena_alloc(parse_arena, sizeof(*member));
    ParseList *result = list_new();
    size_t index;

    if (member == NULL || result == NULL) {
        return NULL;
    }
    member->type = spec->type;
    member->declarator_count = declarators == NULL ? 0 : declarators->count;
    member->declarators = (AstVarDecl **)ast_arena_alloc(parse_arena,
        member->declarator_count * sizeof(*member->declarators));
    if (member->declarator_count != 0 && member->declarators == NULL) {
        return NULL;
    }
    for (index = 0; index < member->declarator_count; ++index) {
        ParseDeclarator *decl = (ParseDeclarator *)declarators->items[index];
        decl->type = complete_type(decl->type, spec->type,
                                   decl->pointer_count);
        member->declarators[index] = (AstVarDecl *)variable_decl(
            decl->type, decl->name, NULL, AST_STORAGE_NONE);
        if (member->declarators[index] == NULL) {
            return NULL;
        }
    }
    if (list_append(result, member) == NULL) {
        return NULL;
    }
    return result;
}

static AstType *structure_type(int is_union, const char *tag, ParseList *members)
{
    AstType *type = (AstType *)ast_arena_alloc(parse_arena, sizeof(*type));
    size_t index;

    if (type == NULL) {
        return NULL;
    }
    if (is_union) {
        AstUnionDecl *declaration =
            (AstUnionDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
        if (declaration == NULL) {
            return NULL;
        }
        declaration->declaration.node.kind = AST_NODE_UNION_DECL;
        declaration->tag = tag;
        declaration->is_complete = (unsigned char)(members != NULL);
        declaration->member_count = members == NULL ? 0 : members->count;
        declaration->members = (AstStructMember *)ast_arena_alloc(parse_arena,
            declaration->member_count * sizeof(*declaration->members));
        for (index = 0; declaration->members != NULL &&
             index < declaration->member_count; ++index) {
            declaration->members[index] =
                *(AstStructMember *)members->items[index];
        }
        type->kind = AST_TYPE_UNION;
        type->as.union_type.declaration = declaration;
    } else {
        AstStructDecl *declaration =
            (AstStructDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
        if (declaration == NULL) {
            return NULL;
        }
        declaration->declaration.node.kind = AST_NODE_STRUCT_DECL;
        declaration->tag = tag;
        declaration->is_complete = (unsigned char)(members != NULL);
        declaration->member_count = members == NULL ? 0 : members->count;
        declaration->members = (AstStructMember *)ast_arena_alloc(parse_arena,
            declaration->member_count * sizeof(*declaration->members));
        for (index = 0; declaration->members != NULL &&
             index < declaration->member_count; ++index) {
            declaration->members[index] =
                *(AstStructMember *)members->items[index];
        }
        type->kind = AST_TYPE_STRUCT;
        type->as.structure.declaration = declaration;
    }
    return type;
}

static AstEnumerator *new_enumerator(const char *name, AstExpr *value)
{
    AstEnumerator *enumerator =
        (AstEnumerator *)ast_arena_alloc(parse_arena, sizeof(*enumerator));
    if (enumerator != NULL) {
        enumerator->node.kind = AST_NODE_ENUMERATOR;
        enumerator->name = name;
        enumerator->value = value;
    }
    return enumerator;
}

static unsigned long decode_escape(const char *text, size_t *index)
{
    unsigned long value;
    size_t digits;

    switch (text[*index]) {
    case 'a': ++*index; return '\a';
    case 'b': ++*index; return '\b';
    case 'f': ++*index; return '\f';
    case 'n': ++*index; return '\n';
    case 'r': ++*index; return '\r';
    case 't': ++*index; return '\t';
    case 'v': ++*index; return '\v';
    case '\\': ++*index; return '\\';
    case '\'': ++*index; return '\'';
    case '"': ++*index; return '"';
    case '?': ++*index; return '?';
    case 'x':
        ++*index;
        value = 0;
        digits = 0;
        while (text[*index] != '\0') {
            unsigned char character = (unsigned char)text[*index];
            unsigned int digit;
            if (character >= '0' && character <= '9') {
                digit = character - '0';
            } else if (character >= 'a' && character <= 'f') {
                digit = character - 'a' + 10;
            } else if (character >= 'A' && character <= 'F') {
                digit = character - 'A' + 10;
            } else {
                break;
            }
            value = (value << 4) | digit;
            ++*index;
            ++digits;
        }
        return digits == 0 ? 'x' : value;
    default:
        if (text[*index] >= '0' && text[*index] <= '7') {
            value = 0;
            digits = 0;
            while (digits < 3 && text[*index] >= '0' && text[*index] <= '7') {
                value = (value << 3) | (unsigned int)(text[*index] - '0');
                ++*index;
                ++digits;
            }
            return value;
        }
        value = (unsigned char)text[*index];
        if (text[*index] != '\0') {
            ++*index;
        }
        return value;
    }
}

static unsigned long character_value(const char *text)
{
    size_t index = 1;

    if (text[index] == '\\') {
        ++index;
        return decode_escape(text, &index);
    }
    return (unsigned long)(unsigned char)text[index];
}

static AstStringExpr *decode_string_literal(const char *text)
{
    size_t spelling_length = strlen(text);
    size_t source_index = 1;
    size_t output_length = 0;
    unsigned char *bytes = (unsigned char *)ast_arena_alloc(
        parse_arena, spelling_length + 1);
    AstStringExpr *expression =
        (AstStringExpr *)new_expr(AST_NODE_STRING_EXPR, sizeof(*expression));

    if (bytes == NULL || expression == NULL) {
        return NULL;
    }
    while (source_index + 1 < spelling_length) {
        if (text[source_index] == '\\') {
            ++source_index;
            bytes[output_length++] =
                (unsigned char)decode_escape(text, &source_index);
        } else {
            bytes[output_length++] = (unsigned char)text[source_index++];
        }
    }
    expression->bytes = bytes;
    expression->length = output_length;
    return expression;
}

static AstStringExpr *concat_string_literals(AstStringExpr *left,
                                            const char *right_text)
{
    AstStringExpr *right = decode_string_literal(right_text);
    AstStringExpr *joined;
    unsigned char *bytes;
    size_t length;

    if (right == NULL) {
        return NULL;
    }
    length = left->length + right->length;
    bytes = (unsigned char *)ast_arena_alloc(parse_arena, length);
    joined = (AstStringExpr *)new_expr(AST_NODE_STRING_EXPR, sizeof(*joined));
    if (bytes == NULL || joined == NULL) {
        return NULL;
    }
    memcpy(bytes, left->bytes, left->length);
    memcpy(bytes + left->length, right->bytes, right->length);
    joined->bytes = bytes;
    joined->length = length;
    return joined;
}

static long integer_value(const char *spelling)
{
    if (spelling[0] == '0' && (spelling[1] == 'b' || spelling[1] == 'B')) {
        return strtol(spelling + 2, NULL, 2);
    }
    return strtol(spelling, NULL, 0);
}

static AstBlockItem *block_declaration(AstDecl *declaration)
{
    AstBlockItem *item = (AstBlockItem *)ast_arena_alloc(parse_arena, sizeof(*item));
    if (item != NULL) {
        item->kind = AST_BLOCK_DECLARATION;
        item->as.declaration = declaration;
    }
    return item;
}

static AstBlockItem *block_statement(AstStmt *statement)
{
    AstBlockItem *item = (AstBlockItem *)ast_arena_alloc(parse_arena, sizeof(*item));
    if (item != NULL) {
        item->kind = AST_BLOCK_STATEMENT;
        item->as.statement = statement;
    }
    return item;
}

static AstStmt *expression_statement(AstExpr *expression)
{
    if (expression == NULL) {
        return new_stmt(AST_NODE_EMPTY_STMT, sizeof(AstEmptyStmt));
    }
    {
        AstExprStmt *statement =
            (AstExprStmt *)new_stmt(AST_NODE_EXPR_STMT, sizeof(*statement));
        if (statement != NULL) {
            statement->expression = expression;
        }
        return (AstStmt *)statement;
    }
}

static ParseSpec *new_spec(AstType *type)
{
    ParseSpec *spec = (ParseSpec *)ast_arena_alloc(parse_arena, sizeof(*spec));
    if (spec != NULL) {
        spec->type = type;
    }
    return spec;
}

static ParseDeclarator *new_declarator(const char *name, AstType *type)
{
    ParseDeclarator *declarator =
        (ParseDeclarator *)ast_arena_alloc(parse_arena, sizeof(*declarator));
    if (declarator != NULL) {
        declarator->name = name;
        declarator->type = type;
    }
    return declarator;
}

static AstDeclList declaration_list(ParseList *items)
{
    AstDeclList result;
    result.items = (AstDecl **)ast_arena_alloc(parse_arena,
        (items == NULL ? 0 : items->count) * sizeof(*result.items));
    result.count = items == NULL ? 0 : items->count;
    if (result.items != NULL && result.count != 0) {
        memcpy(result.items, items->items, result.count * sizeof(*result.items));
    }
    return result;
}

static AstCompoundStmt *compound_statement(ParseList *items)
{
    AstCompoundStmt *compound =
        (AstCompoundStmt *)new_stmt(AST_NODE_COMPOUND_STMT, sizeof(*compound));
    size_t index;

    if (compound == NULL) {
        return NULL;
    }
    compound->item_count = items == NULL ? 0 : items->count;
    compound->items = (AstBlockItem *)ast_arena_alloc(parse_arena,
        compound->item_count * sizeof(*compound->items));
    for (index = 0; compound->items != NULL && index < compound->item_count; ++index) {
        compound->items[index] = *(AstBlockItem *)items->items[index];
    }
    return compound;
}

static ParseList *declaration_items(ParseSpec *spec, ParseList *declarators)
{
    ParseList *items = list_new();
    size_t index;

    if (declarators == NULL) {
        return items;
    }
    for (index = 0; index < declarators->count; ++index) {
        ParseDeclarator *decl = (ParseDeclarator *)declarators->items[index];
        AstDecl *node;
        decl->type = complete_type(decl->type, spec->type,
                                   decl->pointer_count);
        if (spec->is_typedef) {
            AstTypedefDecl *typedef_decl =
                (AstTypedefDecl *)ast_arena_alloc(parse_arena, sizeof(*typedef_decl));
            if (typedef_decl == NULL) {
                return NULL;
            }
            typedef_decl->declaration.node.kind = AST_NODE_TYPEDEF_DECL;
            typedef_decl->type = decl->type;
            typedef_decl->alias = decl->name;
            node = (AstDecl *)typedef_decl;
        } else if (decl->is_function) {
            node = function_decl(decl, decl->type, NULL, spec->storage);
        } else {
            node = variable_decl(decl->type, decl->name, decl->initializer,
                                 spec->storage);
            if (node != NULL) {
                ((AstVarDecl *)node)->attributes = spec->attributes;
            }
        }
        if (node == NULL || list_append(items, node) == NULL) {
            return NULL;
        }
    }
    return items;
}

static int parse_error_reported;
%}

%define parse.error verbose
%define api.token.prefix {LCC2_BISON_}

%union {
    const char *text;
    AstExpr *expression;
    AstStmt *statement;
    AstType *type;
    AstDecl *declaration;
    AstInitializer *initializer;
    AstCompoundStmt *compound;
    AstBlockItem *block_item;
    ParseList *list;
    ParseSpec *spec;
    ParseDeclarator *declarator;
}

%token LCC2_TOK_LEX_ERROR 258
%token <text> LCC2_TOK_IDENTIFIER 259 LCC2_TOK_INTEGER_CONSTANT 260
%token <text> LCC2_TOK_FLOATING_CONSTANT 261 LCC2_TOK_CHARACTER_CONSTANT 262
%token <text> LCC2_TOK_STRING_LITERAL 263 LCC2_TOK_TYPEDEF_NAME 264
%token LCC2_TOK_PTR_OP 265 LCC2_TOK_INC_OP 266 LCC2_TOK_DEC_OP 267
%token LCC2_TOK_LEFT_OP 268 LCC2_TOK_RIGHT_OP 269 LCC2_TOK_LE_OP 270
%token LCC2_TOK_GE_OP 271 LCC2_TOK_EQ_OP 272 LCC2_TOK_NE_OP 273
%token LCC2_TOK_AND_OP 274 LCC2_TOK_OR_OP 275 LCC2_TOK_MUL_ASSIGN 276
%token LCC2_TOK_DIV_ASSIGN 277 LCC2_TOK_MOD_ASSIGN 278 LCC2_TOK_ADD_ASSIGN 279
%token LCC2_TOK_SUB_ASSIGN 280 LCC2_TOK_LEFT_ASSIGN 281
%token LCC2_TOK_RIGHT_ASSIGN 282 LCC2_TOK_AND_ASSIGN 283
%token LCC2_TOK_XOR_ASSIGN 284 LCC2_TOK_OR_ASSIGN 285
%token LCC2_TOK_TYPEDEF 286 LCC2_TOK_EXTERN 287 LCC2_TOK_STATIC 288
%token LCC2_TOK_VOID 289 LCC2_TOK_CHAR 290 LCC2_TOK_INT 291
%token LCC2_TOK_FLOAT 292 LCC2_TOK_SIGNED 293 LCC2_TOK_UNSIGNED 294
%token LCC2_TOK_CONST 295 LCC2_TOK_STRUCT 296 LCC2_TOK_UNION 297
%token LCC2_TOK_ENUM 298 LCC2_TOK_CASE 299 LCC2_TOK_DEFAULT 300
%token LCC2_TOK_IF 301 LCC2_TOK_ELSE 302 LCC2_TOK_SWITCH 303
%token LCC2_TOK_WHILE 304 LCC2_TOK_DO 305 LCC2_TOK_FOR 306
%token LCC2_TOK_GOTO 307 LCC2_TOK_CONTINUE 308 LCC2_TOK_BREAK 309
%token LCC2_TOK_RETURN 310 LCC2_TOK_SIZEOF 311 LCC2_TOK_AUTO 312
%token LCC2_TOK_REGISTER 313 LCC2_TOK_SHORT 314 LCC2_TOK_LONG 315
%token LCC2_TOK_VOLATILE 316 LCC2_TOK_AT 317 LCC2_TOK_XRAM 318
%token LCC2_TOK_ASM 319
%token <text> LCC2_TOK_ASM_TEXT 320
%token LCC2_TOK_PRAGMA_ORG 321 LCC2_TOK_PRAGMA_PC 322
%token LCC2_TOK_PRAGMA_NOSAVE 323

%type <list> translation_unit external_declaration declaration
%type <list> init_declarator_list init_declarator_list_opt block_item_list_opt
%type <list> block_declaration_list block_statement_list
%type <list> parameter_list parameter_list_opt
%type <list> argument_expression_list argument_expression_list_opt
%type <list> pointer_opt
%type <spec> declaration_specifiers
%type <type> type_specifier type_name
%type <declarator> declarator direct_declarator
%type <declaration> parameter_declaration
%type <initializer> initializer
%type <list> initializer_list
%type <list> struct_member_list struct_declarator_list enumerator_list
%type <expression> expression expression_opt assignment_expression
%type <expression> conditional_expression logical_or_expression
%type <expression> logical_and_expression inclusive_or_expression
%type <expression> exclusive_or_expression and_expression equality_expression
%type <expression> relational_expression shift_expression additive_expression
%type <expression> multiplicative_expression cast_expression unary_expression
%type <expression> postfix_expression
%type <expression> primary_expression string_literal_sequence
%type <statement> statement expression_statement selection_statement
%type <statement> iteration_statement jump_statement labeled_statement
%type <statement> asm_statement
%type <compound> compound_statement
%type <text> identifier

%precedence LOWER_THAN_ELSE
%precedence LCC2_TOK_ELSE

%start translation_unit

%%

translation_unit
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; parsed_declarations = $$; }
    | translation_unit external_declaration
      {
          size_t i;
          $$ = $1;
          if ($2 != NULL) {
              for (i = 0; i < $2->count; ++i) {
                  if (list_append($$, $2->items[i]) == NULL) {
                      YYABORT;
                  }
              }
              parsed_declarations = $$;
          }
      }
    ;

external_declaration
    : declaration { $$ = $1; }
    | declaration_specifiers declarator compound_statement
      {
          AstDecl *function = function_decl(
              $2, complete_type($2->type, $1->type, $2->pointer_count),
              $3, $1->storage);
          $$ = list_new();
          if (function == NULL || list_append($$, function) == NULL) YYABORT;
      }
    | LCC2_TOK_PRAGMA_ORG '(' expression ')'
      { parsed_unit->origin = $3; $$ = list_new(); }
    | LCC2_TOK_PRAGMA_PC '(' expression ')'
      { parsed_unit->has_pc_pragma = 1; $$ = list_new(); }
    | LCC2_TOK_PRAGMA_NOSAVE
      { parsed_unit->no_save = 1; $$ = list_new(); }
    ;

declaration
    : declaration_specifiers init_declarator_list_opt
      {
          size_t index;
          if ($1->is_typedef) {
              for (index = 0; index < $2->count; ++index) {
                  if (!register_typedef((ParseDeclarator *)$2->items[index],
                                        $1->type)) {
                      YYABORT;
                  }
              }
          }
      }
      ';'
      {
          $$ = declaration_items($1, $2);
          if ($$ == NULL) YYABORT;
      }
    ;

declaration_specifiers
    : type_specifier { $$ = new_spec($1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_TYPEDEF type_specifier
      { $$ = new_spec($2); if ($$ == NULL) YYABORT; $$->is_typedef = 1; }
    | LCC2_TOK_STATIC type_specifier
      { $$ = new_spec($2); if ($$ == NULL) YYABORT; $$->storage = AST_STORAGE_STATIC; }
    | LCC2_TOK_EXTERN type_specifier
      { $$ = new_spec($2); if ($$ == NULL) YYABORT; $$->storage = AST_STORAGE_EXTERN; }
    | LCC2_TOK_CONST type_specifier { $$ = new_spec($2); if ($$ == NULL) YYABORT; $$->type->qualifiers |= AST_QUALIFIER_CONST; }
    | LCC2_TOK_AT '(' conditional_expression ')' type_specifier
      {
          $$ = new_spec($5);
          if ($$ == NULL) YYABORT;
          $$->attributes.has_address = 1;
          $$->attributes.address = $3;
      }
    | LCC2_TOK_XRAM type_specifier
      {
          $$ = new_spec($2);
          if ($$ == NULL) YYABORT;
          $$->attributes.is_xram = 1;
      }
    | LCC2_TOK_XRAM LCC2_TOK_AT '(' conditional_expression ')' type_specifier
      {
          $$ = new_spec($6);
          if ($$ == NULL) YYABORT;
          $$->attributes.is_xram = 1;
          $$->attributes.has_address = 1;
          $$->attributes.address = $4;
      }
    | LCC2_TOK_AT '(' conditional_expression ')' LCC2_TOK_XRAM type_specifier
      {
          $$ = new_spec($6);
          if ($$ == NULL) YYABORT;
          $$->attributes.is_xram = 1;
          $$->attributes.has_address = 1;
          $$->attributes.address = $3;
      }
    | type_specifier LCC2_TOK_AT '(' conditional_expression ')'
      {
          $$ = new_spec($1);
          if ($$ == NULL) YYABORT;
          $$->attributes.has_address = 1;
          $$->attributes.address = $4;
      }
    | type_specifier LCC2_TOK_XRAM
      {
          $$ = new_spec($1);
          if ($$ == NULL) YYABORT;
          $$->attributes.is_xram = 1;
      }
    | LCC2_TOK_LONG { yyerror("keyword 'long' is unsupported on this platform"); YYABORT; }
    | LCC2_TOK_SHORT { yyerror("keyword 'short' is unsupported on this platform"); YYABORT; }
    | LCC2_TOK_VOLATILE { yyerror("keyword 'volatile' is unsupported on this platform"); YYABORT; }
    | LCC2_TOK_REGISTER { yyerror("keyword 'register' is unsupported on this platform"); YYABORT; }
    | LCC2_TOK_AUTO { yyerror("keyword 'auto' is unsupported on this platform"); YYABORT; }
    ;

type_specifier
    : LCC2_TOK_VOID { $$ = base_type(AST_TYPE_VOID, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_CHAR { $$ = base_type(AST_TYPE_CHAR, 1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_INT { $$ = base_type(AST_TYPE_INT, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_FLOAT { $$ = base_type(AST_TYPE_FLOAT, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_SIGNED { $$ = base_type(AST_TYPE_INT, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNSIGNED { $$ = base_type(AST_TYPE_INT, 1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_SIGNED LCC2_TOK_CHAR { $$ = base_type(AST_TYPE_CHAR, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNSIGNED LCC2_TOK_CHAR { $$ = base_type(AST_TYPE_CHAR, 1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_SIGNED LCC2_TOK_INT { $$ = base_type(AST_TYPE_INT, 0); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNSIGNED LCC2_TOK_INT { $$ = base_type(AST_TYPE_INT, 1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_TYPEDEF_NAME
      { $$ = typedef_type($1); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_STRUCT identifier '{' struct_member_list '}'
      { $$ = structure_type(0, $2, $4); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_STRUCT '{' struct_member_list '}'
      { $$ = structure_type(0, NULL, $3); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_STRUCT identifier
      { $$ = structure_type(0, $2, NULL); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNION identifier '{' struct_member_list '}'
      { $$ = structure_type(1, $2, $4); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNION '{' struct_member_list '}'
      { $$ = structure_type(1, NULL, $3); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_UNION identifier
      { $$ = structure_type(1, $2, NULL); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_ENUM identifier '{' enumerator_list comma_opt '}'
      {
          AstEnumDecl *declaration =
              (AstEnumDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
          size_t i;
          if (declaration == NULL) YYABORT;
          declaration->declaration.node.kind = AST_NODE_ENUM_DECL;
          declaration->tag = $2;
          declaration->is_complete = 1;
          declaration->enumerator_count = $4->count;
          declaration->enumerators = (AstEnumerator **)ast_arena_alloc(parse_arena,
              declaration->enumerator_count * sizeof(*declaration->enumerators));
          if (declaration->enumerators == NULL) YYABORT;
          for (i = 0; i < declaration->enumerator_count; ++i)
              declaration->enumerators[i] = (AstEnumerator *)$4->items[i];
          $$ = (AstType *)ast_arena_alloc(parse_arena, sizeof(*$$));
          if ($$ == NULL) YYABORT;
          $$->kind = AST_TYPE_ENUM;
          $$->as.enumeration.declaration = declaration;
      }
    | LCC2_TOK_ENUM '{' enumerator_list comma_opt '}'
      {
          AstEnumDecl *declaration =
              (AstEnumDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
          size_t i;
          if (declaration == NULL) YYABORT;
          declaration->declaration.node.kind = AST_NODE_ENUM_DECL;
          declaration->is_complete = 1;
          declaration->enumerator_count = $3->count;
          declaration->enumerators = (AstEnumerator **)ast_arena_alloc(parse_arena,
              declaration->enumerator_count * sizeof(*declaration->enumerators));
          if (declaration->enumerators == NULL) YYABORT;
          for (i = 0; i < declaration->enumerator_count; ++i)
              declaration->enumerators[i] = (AstEnumerator *)$3->items[i];
          $$ = (AstType *)ast_arena_alloc(parse_arena, sizeof(*$$));
          if ($$ == NULL) YYABORT;
          $$->kind = AST_TYPE_ENUM;
          $$->as.enumeration.declaration = declaration;
      }
    | LCC2_TOK_ENUM identifier
      {
          AstEnumDecl *declaration =
              (AstEnumDecl *)ast_arena_alloc(parse_arena, sizeof(*declaration));
          if (declaration == NULL) YYABORT;
          declaration->declaration.node.kind = AST_NODE_ENUM_DECL;
          declaration->tag = $2;
          $$ = (AstType *)ast_arena_alloc(parse_arena, sizeof(*$$));
          if ($$ == NULL) YYABORT;
          $$->kind = AST_TYPE_ENUM;
          $$->as.enumeration.declaration = declaration;
      }
    ;

struct_member_list
    : declaration_specifiers struct_declarator_list ';'
      { $$ = make_struct_member($1, $2); if ($$ == NULL) YYABORT; }
    | struct_member_list declaration_specifiers struct_declarator_list ';'
      {
          size_t i;
          $$ = $1;
          {
              ParseList *members = make_struct_member($2, $3);
              if (members == NULL) YYABORT;
              for (i = 0; i < members->count; ++i)
                  if (list_append($$, members->items[i]) == NULL) YYABORT;
          }
      }
    ;

struct_declarator_list
    : declarator
      { $$ = list_new(); if ($$ == NULL || list_append($$, $1) == NULL) YYABORT; }
    | struct_declarator_list ',' declarator
      { $$ = $1; if (list_append($$, $3) == NULL) YYABORT; }
    ;

enumerator_list
    : identifier
      {
          $$ = list_new();
          if ($$ == NULL || list_append($$, new_enumerator($1, NULL)) == NULL) YYABORT;
      }
    | identifier '=' conditional_expression
      {
          $$ = list_new();
          if ($$ == NULL || list_append($$, new_enumerator($1, $3)) == NULL) YYABORT;
      }
    | enumerator_list ',' identifier
      {
          $$ = $1;
          if (list_append($$, new_enumerator($3, NULL)) == NULL) YYABORT;
      }
    | enumerator_list ',' identifier '=' conditional_expression
      {
          $$ = $1;
          if (list_append($$, new_enumerator($3, $5)) == NULL) YYABORT;
      }
    ;

init_declarator_list_opt
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | init_declarator_list { $$ = $1; }
    ;

init_declarator_list
    : declarator
      {
          $$ = list_new();
          if ($$ == NULL || list_append($$, $1) == NULL) YYABORT;
      }
    | init_declarator_list ',' declarator
      { $$ = $1; if (list_append($$, $3) == NULL) YYABORT; }
    | declarator '=' initializer
      {
          ParseDeclarator *decl = $1;
          $$ = list_new();
          if ($$ == NULL) YYABORT;
          decl->initializer = $3;
          if (list_append($$, decl) == NULL) YYABORT;
      }
    | init_declarator_list ',' declarator '=' initializer
      {
          ParseDeclarator *decl = $3;
          $$ = $1;
          decl->initializer = $5;
          if (list_append($$, decl) == NULL) YYABORT;
      }
    ;

declarator
    : pointer_opt direct_declarator
      {
          $$ = $2;
          if ($$ == NULL) YYABORT;
          $$->pointer_count = $1 == NULL ? 0 : $1->count;
      }
    ;

pointer_opt
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | '*' pointer_opt
      { $$ = $2; if (list_append($$, (void *)1) == NULL) YYABORT; }
    ;

direct_declarator
    : identifier
      {
          $$ = new_declarator($1, NULL);
          if ($$ == NULL) YYABORT;
      }
    | direct_declarator '[' ']'
      {
          AstType *array_type = (AstType *)ast_arena_alloc(parse_arena, sizeof(*array_type));
          $$ = $1;
          if ($$->type != NULL && $$->type->kind == AST_TYPE_ARRAY) {
              yyerror("multidimensional arrays are unsupported");
              YYABORT;
          }
          if (array_type == NULL) YYABORT;
          array_type->kind = AST_TYPE_ARRAY;
          array_type->as.array.element = $$->type;
          $$->type = array_type;
      }
    | direct_declarator '[' conditional_expression ']'
      {
          AstType *array_type = (AstType *)ast_arena_alloc(parse_arena, sizeof(*array_type));
          $$ = $1;
          if ($$->type != NULL && $$->type->kind == AST_TYPE_ARRAY) {
              yyerror("multidimensional arrays are unsupported");
              YYABORT;
          }
          if (array_type == NULL) YYABORT;
          array_type->kind = AST_TYPE_ARRAY;
          array_type->as.array.element = $$->type;
          array_type->as.array.length = $3;
          $$->type = array_type;
      }
    | direct_declarator '(' parameter_list_opt ')'
      {
          size_t i;
          $$ = $1;
          $$->is_function = 1;
          $$->parameter_count = $3 == NULL ? 0 : $3->count;
          if ($$->parameter_count != 0) {
              $$->parameters = (AstParamDecl **)ast_arena_alloc(parse_arena,
                  $$->parameter_count * sizeof(*$$->parameters));
              if ($$->parameters == NULL) YYABORT;
              for (i = 0; i < $$->parameter_count; ++i) {
                  $$->parameters[i] = (AstParamDecl *)$3->items[i];
              }
          }
      }
    ;

parameter_list_opt
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | parameter_list { $$ = $1; }
    ;

parameter_list
    : parameter_declaration
      {
          $$ = list_new();
          if ($$ == NULL || list_append($$, $1) == NULL) YYABORT;
      }
    | parameter_list ',' parameter_declaration
      { $$ = $1; if (list_append($$, $3) == NULL) YYABORT; }
    ;

parameter_declaration
    : declaration_specifiers declarator
      {
          $$ = make_parameter($2, $1->type);
          if ($$ == NULL) YYABORT;
          ((AstParamDecl *)$$)->type = complete_type(
              $2->type, $1->type, $2->pointer_count);
      }
    | declaration_specifiers
      {
          $$ = make_parameter(NULL, $1->type);
          if ($$ == NULL) YYABORT;
      }
    ;

initializer
    : assignment_expression
      { $$ = expression_initializer($1); if ($$ == NULL) YYABORT; }
    | '{' initializer_list comma_opt '}'
      { $$ = list_initializer($2); if ($$ == NULL) YYABORT; }
    ;

initializer_list
    : initializer
      { $$ = list_new(); if ($$ == NULL || list_append($$, $1) == NULL) YYABORT; }
    | initializer_list ',' initializer
      { $$ = $1; if (list_append($$, $3) == NULL) YYABORT; }
    ;

comma_opt
    : %empty
    | ','
    ;

compound_statement
    : '{' { if (!push_typedef_scope()) YYABORT; }
      block_item_list_opt '}'
      { pop_typedef_scope(); $$ = compound_statement($3); if ($$ == NULL) YYABORT; }
    ;

block_declaration_list
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | block_declaration_list declaration
      {
          size_t i;
          $$ = $1;
          for (i = 0; i < $2->count; ++i) {
              AstBlockItem *item = block_declaration($2->items[i]);
              if (item == NULL || list_append($$, item) == NULL) YYABORT;
          }
      }
    ;

block_statement_list
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | block_statement_list statement
      {
          AstBlockItem *item = block_statement($2);
          $$ = $1;
          if (item == NULL || list_append($$, item) == NULL) YYABORT;
      }
    ;

block_item_list_opt
    : block_declaration_list block_statement_list
      {
          size_t i;
          $$ = $1;
          for (i = 0; i < $2->count; ++i) {
              if (list_append($$, $2->items[i]) == NULL) YYABORT;
          }
      }
    ;

/* C89 requires declarations to precede statements in each compound block. */

statement
    : compound_statement { $$ = (AstStmt *)$1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
    | labeled_statement { $$ = $1; }
    | asm_statement { $$ = $1; }
    ;

expression_statement
    : expression_opt ';' { $$ = expression_statement($1); if ($$ == NULL) YYABORT; }
    ;

expression_opt
    : %empty { $$ = NULL; }
    | expression { $$ = $1; }
    ;

selection_statement
    : LCC2_TOK_IF '(' expression ')' statement %prec LOWER_THAN_ELSE
      {
          AstIfStmt *node = (AstIfStmt *)new_stmt(AST_NODE_IF_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->condition = $3;
          node->then_branch = $5;
          $$ = (AstStmt *)node;
      }
    | LCC2_TOK_IF '(' expression ')' statement LCC2_TOK_ELSE statement
      {
          AstIfStmt *node = (AstIfStmt *)new_stmt(AST_NODE_IF_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->condition = $3;
          node->then_branch = $5;
          node->else_branch = $7;
          $$ = (AstStmt *)node;
      }
    | LCC2_TOK_SWITCH '(' expression ')' statement
      {
          AstSwitchStmt *node = (AstSwitchStmt *)new_stmt(AST_NODE_SWITCH_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->expression = $3;
          node->body = $5;
          $$ = (AstStmt *)node;
      }
    ;

iteration_statement
    : LCC2_TOK_WHILE '(' expression ')' statement
      {
          AstWhileStmt *node = (AstWhileStmt *)new_stmt(AST_NODE_WHILE_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->condition = $3; node->body = $5; $$ = (AstStmt *)node;
      }
    | LCC2_TOK_DO statement LCC2_TOK_WHILE '(' expression ')' ';'
      {
          AstDoWhileStmt *node = (AstDoWhileStmt *)new_stmt(AST_NODE_DO_WHILE_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->body = $2; node->condition = $5; $$ = (AstStmt *)node;
      }
    | LCC2_TOK_FOR '(' expression_opt ';' expression_opt ';' expression_opt ')' statement
      {
          AstForStmt *node = (AstForStmt *)new_stmt(AST_NODE_FOR_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->init_expression = $3; node->condition = $5;
          node->step = $7; node->body = $9; $$ = (AstStmt *)node;
      }
    ;

jump_statement
    : LCC2_TOK_GOTO identifier ';'
      {
          AstGotoStmt *node = (AstGotoStmt *)new_stmt(AST_NODE_GOTO_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->label = $2; $$ = (AstStmt *)node;
      }
    | LCC2_TOK_CONTINUE ';' { $$ = new_stmt(AST_NODE_CONTINUE_STMT, sizeof(AstContinueStmt)); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_BREAK ';' { $$ = new_stmt(AST_NODE_BREAK_STMT, sizeof(AstBreakStmt)); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_RETURN expression_opt ';'
      {
          AstReturnStmt *node = (AstReturnStmt *)new_stmt(AST_NODE_RETURN_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->expression = $2; $$ = (AstStmt *)node;
      }
    ;

labeled_statement
    : identifier ':' statement
      {
          AstLabelStmt *node = (AstLabelStmt *)new_stmt(AST_NODE_LABEL_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->name = $1; node->body = $3; $$ = (AstStmt *)node;
      }
    | LCC2_TOK_CASE conditional_expression ':' statement
      {
          AstCaseStmt *node = (AstCaseStmt *)new_stmt(AST_NODE_CASE_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->value = $2; node->body = $4; $$ = (AstStmt *)node;
      }
    | LCC2_TOK_DEFAULT ':' statement
      {
          AstDefaultStmt *node = (AstDefaultStmt *)new_stmt(AST_NODE_DEFAULT_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->body = $3; $$ = (AstStmt *)node;
      }
    ;

asm_statement
    : LCC2_TOK_ASM '{' '}'
      {
          AstAsmStmt *node = (AstAsmStmt *)new_stmt(AST_NODE_ASM_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          $$ = (AstStmt *)node;
      }
    | LCC2_TOK_ASM '{' LCC2_TOK_ASM_TEXT '}'
      {
          AstAsmStmt *node = (AstAsmStmt *)new_stmt(AST_NODE_ASM_STMT, sizeof(*node));
          if (node == NULL) YYABORT;
          node->code = copy_text($3, strlen($3));
          node->code_length = strlen($3);
          $$ = (AstStmt *)node;
      }
    ;

primary_expression
    : identifier
      {
          AstIdentExpr *node = (AstIdentExpr *)new_expr(AST_NODE_IDENT_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->name = $1; $$ = (AstExpr *)node;
      }
    | LCC2_TOK_INTEGER_CONSTANT
      {
          AstConstantExpr *node = (AstConstantExpr *)new_expr(AST_NODE_CONSTANT_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->constant_kind = AST_CONSTANT_INTEGER;
          node->spelling = copy_text($1, strlen($1));
          node->type = base_type(AST_TYPE_INT,
                                 strpbrk(node->spelling, "uU") != NULL);
          if (node->spelling == NULL || node->type == NULL) YYABORT;
          node->value.integer = integer_value(node->spelling);
          $$ = (AstExpr *)node;
      }
    | LCC2_TOK_FLOATING_CONSTANT
      {
          AstConstantExpr *node = (AstConstantExpr *)new_expr(AST_NODE_CONSTANT_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->constant_kind = AST_CONSTANT_FLOAT;
          node->spelling = copy_text($1, strlen($1));
          node->type = base_type(AST_TYPE_FLOAT, 0);
          if (node->spelling == NULL || node->type == NULL) YYABORT;
          node->value.floating = strtod(node->spelling, NULL);
          $$ = (AstExpr *)node;
      }
    | LCC2_TOK_CHARACTER_CONSTANT
      {
          AstConstantExpr *node = (AstConstantExpr *)new_expr(AST_NODE_CONSTANT_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->constant_kind = AST_CONSTANT_CHARACTER;
          node->spelling = copy_text($1, strlen($1));
          node->type = base_type(AST_TYPE_CHAR, 0);
          if (node->spelling == NULL || node->type == NULL) YYABORT;
          node->value.character = character_value($1);
          $$ = (AstExpr *)node;
      }
    | string_literal_sequence { $$ = $1; }
    | '(' expression ')' { $$ = $2; }
    ;

string_literal_sequence
    : LCC2_TOK_STRING_LITERAL
      { $$ = (AstExpr *)decode_string_literal($1); if ($$ == NULL) YYABORT; }
    | string_literal_sequence LCC2_TOK_STRING_LITERAL
      {
          $$ = (AstExpr *)concat_string_literals((AstStringExpr *)$1, $2);
          if ($$ == NULL) YYABORT;
      }
    ;

postfix_expression
    : primary_expression { $$ = $1; }
    | postfix_expression '[' expression ']'
      {
          AstIndexExpr *node = (AstIndexExpr *)new_expr(AST_NODE_INDEX_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->array = $1; node->index = $3; $$ = (AstExpr *)node;
      }
    | postfix_expression '(' argument_expression_list_opt ')'
      {
          AstCallExpr *node = (AstCallExpr *)new_expr(AST_NODE_CALL_EXPR, sizeof(*node));
          size_t i;
          if (node == NULL) YYABORT;
          node->function = $1;
          node->arguments.count = $3 == NULL ? 0 : $3->count;
          node->arguments.items = (AstExpr **)ast_arena_alloc(parse_arena,
              node->arguments.count * sizeof(*node->arguments.items));
          for (i = 0; node->arguments.items != NULL && i < node->arguments.count; ++i)
              node->arguments.items[i] = (AstExpr *)$3->items[i];
          $$ = (AstExpr *)node;
      }
    | postfix_expression '.' identifier
      {
          AstMemberExpr *node = (AstMemberExpr *)new_expr(AST_NODE_MEMBER_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->object = $1; node->field = $3; $$ = (AstExpr *)node;
      }
    | postfix_expression LCC2_TOK_PTR_OP identifier
      {
          AstMemberExpr *node = (AstMemberExpr *)new_expr(AST_NODE_MEMBER_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->object = $1; node->field = $3; node->is_arrow = 1; $$ = (AstExpr *)node;
      }
    | postfix_expression LCC2_TOK_INC_OP { $$ = unary_expr(AST_UNARY_POST_INCREMENT, $1); if ($$ == NULL) YYABORT; }
    | postfix_expression LCC2_TOK_DEC_OP { $$ = unary_expr(AST_UNARY_POST_DECREMENT, $1); if ($$ == NULL) YYABORT; }
    ;

argument_expression_list_opt
    : %empty { $$ = list_new(); if ($$ == NULL) YYABORT; }
    | argument_expression_list { $$ = $1; }
    ;

argument_expression_list
    : assignment_expression
      { $$ = list_new(); if ($$ == NULL || list_append($$, $1) == NULL) YYABORT; }
    | argument_expression_list ',' assignment_expression
      { $$ = $1; if (list_append($$, $3) == NULL) YYABORT; }
    ;

unary_expression
    : postfix_expression { $$ = $1; }
    | LCC2_TOK_INC_OP cast_expression { $$ = unary_expr(AST_UNARY_PRE_INCREMENT, $2); if ($$ == NULL) YYABORT; }
    | LCC2_TOK_DEC_OP cast_expression { $$ = unary_expr(AST_UNARY_PRE_DECREMENT, $2); if ($$ == NULL) YYABORT; }
    | '+' cast_expression { $$ = unary_expr(AST_UNARY_PLUS, $2); if ($$ == NULL) YYABORT; }
    | '-' cast_expression { $$ = unary_expr(AST_UNARY_MINUS, $2); if ($$ == NULL) YYABORT; }
    | '!' cast_expression { $$ = unary_expr(AST_UNARY_LOGICAL_NOT, $2); if ($$ == NULL) YYABORT; }
    | '~' cast_expression { $$ = unary_expr(AST_UNARY_BITWISE_NOT, $2); if ($$ == NULL) YYABORT; }
    | '&' cast_expression
      {
          AstAddressExpr *node = (AstAddressExpr *)new_expr(AST_NODE_ADDRESS_EXPR, sizeof(*node));
          if (node == NULL) YYABORT; node->operand = $2; $$ = (AstExpr *)node;
      }
    | '*' cast_expression
      {
          AstDereferenceExpr *node = (AstDereferenceExpr *)new_expr(AST_NODE_DEREFERENCE_EXPR, sizeof(*node));
          if (node == NULL) YYABORT; node->operand = $2; $$ = (AstExpr *)node;
      }
    | LCC2_TOK_SIZEOF unary_expression
      {
          AstSizeofExpr *node = (AstSizeofExpr *)new_expr(AST_NODE_SIZEOF_EXPR, sizeof(*node));
          if (node == NULL) YYABORT; node->operand.expression = $2; $$ = (AstExpr *)node;
      }
    | LCC2_TOK_SIZEOF '(' type_name ')'
      {
          AstSizeofExpr *node = (AstSizeofExpr *)new_expr(AST_NODE_SIZEOF_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->is_type_operand = 1;
          node->operand.type = $3;
          $$ = (AstExpr *)node;
      }
    ;

cast_expression
    : unary_expression { $$ = $1; }
    | '(' type_name ')' cast_expression
      {
          AstCastExpr *node = (AstCastExpr *)new_expr(AST_NODE_CAST_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->type = $2;
          node->operand = $4;
          $$ = (AstExpr *)node;
      }
    ;

multiplicative_expression
    : cast_expression { $$ = $1; }
    | multiplicative_expression '*' cast_expression { $$ = binary_expr(AST_BINARY_MUL, $1, $3); if ($$ == NULL) YYABORT; }
    | multiplicative_expression '/' cast_expression { $$ = binary_expr(AST_BINARY_DIV, $1, $3); if ($$ == NULL) YYABORT; }
    | multiplicative_expression '%' cast_expression { $$ = binary_expr(AST_BINARY_MOD, $1, $3); if ($$ == NULL) YYABORT; }
    ;

additive_expression
    : multiplicative_expression { $$ = $1; }
    | additive_expression '+' multiplicative_expression { $$ = binary_expr(AST_BINARY_ADD, $1, $3); if ($$ == NULL) YYABORT; }
    | additive_expression '-' multiplicative_expression { $$ = binary_expr(AST_BINARY_SUB, $1, $3); if ($$ == NULL) YYABORT; }
    ;

shift_expression
    : additive_expression { $$ = $1; }
    | shift_expression LCC2_TOK_LEFT_OP additive_expression { $$ = binary_expr(AST_BINARY_SHIFT_LEFT, $1, $3); if ($$ == NULL) YYABORT; }
    | shift_expression LCC2_TOK_RIGHT_OP additive_expression { $$ = binary_expr(AST_BINARY_SHIFT_RIGHT, $1, $3); if ($$ == NULL) YYABORT; }
    ;

relational_expression
    : shift_expression { $$ = $1; }
    | relational_expression '<' shift_expression { $$ = binary_expr(AST_BINARY_LESS, $1, $3); if ($$ == NULL) YYABORT; }
    | relational_expression '>' shift_expression { $$ = binary_expr(AST_BINARY_GREATER, $1, $3); if ($$ == NULL) YYABORT; }
    | relational_expression LCC2_TOK_LE_OP shift_expression { $$ = binary_expr(AST_BINARY_LESS_EQUAL, $1, $3); if ($$ == NULL) YYABORT; }
    | relational_expression LCC2_TOK_GE_OP shift_expression { $$ = binary_expr(AST_BINARY_GREATER_EQUAL, $1, $3); if ($$ == NULL) YYABORT; }
    ;

equality_expression
    : relational_expression { $$ = $1; }
    | equality_expression LCC2_TOK_EQ_OP relational_expression { $$ = binary_expr(AST_BINARY_EQUAL, $1, $3); if ($$ == NULL) YYABORT; }
    | equality_expression LCC2_TOK_NE_OP relational_expression { $$ = binary_expr(AST_BINARY_NOT_EQUAL, $1, $3); if ($$ == NULL) YYABORT; }
    ;

and_expression
    : equality_expression { $$ = $1; }
    | and_expression '&' equality_expression { $$ = binary_expr(AST_BINARY_BIT_AND, $1, $3); if ($$ == NULL) YYABORT; }
    ;

exclusive_or_expression
    : and_expression { $$ = $1; }
    | exclusive_or_expression '^' and_expression { $$ = binary_expr(AST_BINARY_BIT_XOR, $1, $3); if ($$ == NULL) YYABORT; }
    ;

inclusive_or_expression
    : exclusive_or_expression { $$ = $1; }
    | inclusive_or_expression '|' exclusive_or_expression { $$ = binary_expr(AST_BINARY_BIT_OR, $1, $3); if ($$ == NULL) YYABORT; }
    ;

logical_and_expression
    : inclusive_or_expression { $$ = $1; }
    | logical_and_expression LCC2_TOK_AND_OP inclusive_or_expression { $$ = binary_expr(AST_BINARY_LOGICAL_AND, $1, $3); if ($$ == NULL) YYABORT; }
    ;

logical_or_expression
    : logical_and_expression { $$ = $1; }
    | logical_or_expression LCC2_TOK_OR_OP logical_and_expression { $$ = binary_expr(AST_BINARY_LOGICAL_OR, $1, $3); if ($$ == NULL) YYABORT; }
    ;

conditional_expression
    : logical_or_expression { $$ = $1; }
    | logical_or_expression '?' expression ':' conditional_expression
      {
          AstTernaryExpr *node = (AstTernaryExpr *)new_expr(AST_NODE_TERNARY_EXPR, sizeof(*node));
          if (node == NULL) YYABORT;
          node->condition = $1; node->then_expression = $3;
          node->else_expression = $5; $$ = (AstExpr *)node;
      }
    ;

assignment_expression
    : conditional_expression { $$ = $1; }
    | unary_expression '=' assignment_expression { $$ = binary_expr(AST_BINARY_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_MUL_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_MUL_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_DIV_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_DIV_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_MOD_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_MOD_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_ADD_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_ADD_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_SUB_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_SUB_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_LEFT_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_SHIFT_LEFT_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_RIGHT_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_SHIFT_RIGHT_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_AND_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_AND_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_XOR_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_XOR_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    | unary_expression LCC2_TOK_OR_ASSIGN assignment_expression { $$ = binary_expr(AST_BINARY_OR_ASSIGN, $1, $3); if ($$ == NULL) YYABORT; }
    ;

expression
    : assignment_expression { $$ = $1; }
    | expression ',' assignment_expression { $$ = binary_expr(AST_BINARY_COMMA, $1, $3); if ($$ == NULL) YYABORT; }
    ;

type_name
    : type_specifier pointer_opt
      {
          size_t i;
          $$ = $1;
          for (i = 0; i < $2->count; ++i) {
              $$ = pointer_type($$);
              if ($$ == NULL) YYABORT;
          }
      }
    ;

identifier
    : LCC2_TOK_IDENTIFIER { $$ = copy_text($1, strlen($1)); if ($$ == NULL) YYABORT; }
    ;

%%

void yyerror(const char *message)
{
    if (!parse_error_reported) {
        fprintf(stderr, "parser:%lu:%lu: %s\n",
                lcc2_token_line, lcc2_token_column, message);
        parse_error_reported = 1;
    }
}

int lcc2_parse_string(AstArena *arena, const char *source,
                      AstTranslationUnit **translation_unit)
{
    typedef struct yy_buffer_state *YY_BUFFER_STATE;
    extern YY_BUFFER_STATE yy_scan_string(const char *text);
    extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
    extern void lcc2_lexer_reset_location(void);
    YY_BUFFER_STATE buffer;
    int result;

    if (arena == NULL || source == NULL || translation_unit == NULL) {
        return 1;
    }
    *translation_unit = NULL;
    parse_arena = arena;
    parsed_unit = (AstTranslationUnit *)ast_arena_alloc(arena, sizeof(*parsed_unit));
    if (parsed_unit == NULL) {
        parse_arena = NULL;
        return 2;
    }
    parsed_declarations = NULL;
    parse_error_reported = 0;
    free_parse_lists();
    free(typedef_scope_marks);
    typedef_scope_marks = NULL;
    typedef_scope_count = 0;
    typedef_scope_capacity = 0;
    typedef_names = list_new();
    if (typedef_names == NULL) {
        parse_arena = NULL;
        return 2;
    }
    lcc2_lexer_reset_location();
    buffer = yy_scan_string(source);
    if (buffer == NULL) {
        free_parse_lists();
        typedef_names = NULL;
        free(typedef_scope_marks);
        typedef_scope_marks = NULL;
        typedef_scope_count = 0;
        typedef_scope_capacity = 0;
        parse_arena = NULL;
        return 2;
    }
    result = yyparse();
    if (result == 0) {
        parsed_unit->node.kind = AST_NODE_TRANSLATION_UNIT;
        parsed_unit->declarations = declaration_list(parsed_declarations);
        *translation_unit = parsed_unit;
    }
    yy_delete_buffer(buffer);
    free_parse_lists();
    free(typedef_scope_marks);
    typedef_scope_marks = NULL;
    typedef_scope_count = 0;
    typedef_scope_capacity = 0;
    typedef_names = NULL;
    parse_arena = NULL;
    return result;
}
