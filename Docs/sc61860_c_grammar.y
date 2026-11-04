/*
 * Formal syntax for the SC61860 C subset.
 *
 * This file is a Bison grammar reference, not a generated parser. The lexer
 * must distinguish TYPEDEF_NAME from IDENTIFIER using the current typedef
 * scope. It must also recognize multi-character operators as the named
 * tokens below and may normalize #pragma lines into the PRAGMA_* tokens.
 * Type compatibility, constant-expression validity, declaration constraints,
 * and scope checks are semantic-analysis responsibilities.
 *
 * The grammar follows C89 declaration placement: declarations precede
 * statements in a block. Declarators intentionally omit parenthesized
 * declarators, function pointers, and multidimensional arrays.
 */

%token IDENTIFIER TYPEDEF_NAME
%token INTEGER_CONSTANT FLOATING_CONSTANT CHARACTER_CONSTANT STRING_LITERAL

%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN SUB_ASSIGN
%token LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN

%token TYPEDEF EXTERN STATIC
%token VOID CHAR INT FLOAT SIGNED UNSIGNED CONST
%token STRUCT UNION ENUM
%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR
%token GOTO CONTINUE BREAK RETURN SIZEOF

%token AT XRAM ASM ASM_TEXT
%token PRAGMA_ORG PRAGMA_PC PRAGMA_NOSAVE

%precedence LOWER_THAN_ELSE
%precedence ELSE

%start translation_unit

%%

translation_unit
    : external_declaration
    | translation_unit external_declaration
    ;

external_declaration
    : function_definition
    | declaration
    | pragma_directive
    ;

function_definition
    : declaration_specifiers declarator compound_statement
    ;

declaration
    : declaration_specifiers init_declarator_list_opt ';'
    ;

declaration_specifiers
    : declaration_modifiers_opt type_specifier declaration_modifiers_opt
    ;

declaration_modifiers_opt
    : %empty
    | declaration_modifiers
    ;

declaration_modifiers
    : declaration_modifier
    | declaration_modifiers declaration_modifier
    ;

declaration_modifier
    : storage_class_specifier
    | type_qualifier
    | platform_attribute
    ;

storage_class_specifier
    : TYPEDEF
    | EXTERN
    | STATIC
    ;

type_specifier
    : VOID
    | CHAR
    | INT
    | FLOAT
    | SIGNED
    | SIGNED CHAR
    | SIGNED INT
    | CHAR SIGNED
    | INT SIGNED
    | UNSIGNED
    | UNSIGNED CHAR
    | UNSIGNED INT
    | CHAR UNSIGNED
    | INT UNSIGNED
    | TYPEDEF_NAME
    | struct_or_union_specifier
    | enum_specifier
    ;

type_qualifier
    : CONST
    ;

struct_or_union_specifier
    : struct_or_union tag_name_opt '{' struct_declaration_list '}'
    | struct_or_union tag_name
    ;

struct_or_union
    : STRUCT
    | UNION
    ;

struct_declaration_list
    : struct_declaration
    | struct_declaration_list struct_declaration
    ;

struct_declaration
    : declaration_specifiers struct_declarator_list ';'
    ;

struct_declarator_list
    : struct_declarator
    | struct_declarator_list ',' struct_declarator
    ;

struct_declarator
    : declarator
    ;

enum_specifier
    : ENUM tag_name_opt '{' enumerator_list comma_opt '}'
    | ENUM tag_name
    ;

enumerator_list
    : enumerator
    | enumerator_list ',' enumerator
    ;

enumerator
    : declaration_identifier
    | declaration_identifier '=' constant_expression
    ;

init_declarator_list_opt
    : %empty
    | init_declarator_list
    ;

init_declarator_list
    : init_declarator
    | init_declarator_list ',' init_declarator
    ;

init_declarator
    : declarator
    | declarator '=' initializer
    ;

/*
 * Only direct object/function declarators are admitted. Omitting
 * parenthesized declarators excludes function pointers and pointers to
 * arrays; allowing at most one array suffix excludes multidimensional arrays.
 */
declarator
    : pointer_opt direct_declarator
    ;

pointer_opt
    : %empty
    | pointer
    ;

pointer
    : '*' type_qualifier_list_opt pointer_opt
    ;

type_qualifier_list_opt
    : %empty
    | type_qualifier_list
    ;

type_qualifier_list
    : type_qualifier
    | type_qualifier_list type_qualifier
    ;

direct_declarator
    : declaration_identifier
    | declaration_identifier '[' constant_expression_opt ']'
    | direct_declarator '(' parameter_type_list_opt ')'
    ;

declaration_identifier
    : IDENTIFIER
    | TYPEDEF_NAME
    ;

identifier
    : IDENTIFIER
    ;

tag_name
    : IDENTIFIER
    | TYPEDEF_NAME
    ;

tag_name_opt
    : %empty
    | tag_name
    ;

parameter_type_list_opt
    : %empty
    | parameter_list
    ;

parameter_list
    : parameter_declaration
    | parameter_list ',' parameter_declaration
    ;

parameter_declaration
    : declaration_specifiers declarator
    | declaration_specifiers
    ;

platform_attribute
    : AT '(' constant_expression ')'
    | XRAM
    ;

initializer
    : assignment_expression
    | '{' initializer_list comma_opt '}'
    ;

initializer_list
    : initializer
    | initializer_list ',' initializer
    ;

comma_opt
    : %empty
    | ','
    ;

constant_expression_opt
    : %empty
    | constant_expression
    ;

constant_expression
    : conditional_expression
    ;

compound_statement
    : '{' block_item_list_opt '}'
    ;

block_item_list_opt
    : %empty
    | block_item_list
    ;

block_item_list
    : block_item
    | block_item_list block_item
    ;

block_item
    : declaration
    | statement
    ;

statement
    : labeled_statement
    | compound_statement
    | expression_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    | asm_statement
    ;

labeled_statement
    : IDENTIFIER ':' statement
    | CASE constant_expression ':' statement
    | DEFAULT ':' statement
    ;

expression_statement
    : expression_opt ';'
    ;

expression_opt
    : %empty
    | expression
    ;

selection_statement
    : IF '(' expression ')' statement %prec LOWER_THAN_ELSE
    | IF '(' expression ')' statement ELSE statement
    | SWITCH '(' expression ')' statement
    ;

iteration_statement
    : WHILE '(' expression ')' statement
    | DO statement WHILE '(' expression ')' ';'
    | FOR '(' expression_opt ';' expression_opt ';' expression_opt ')' statement
    ;

jump_statement
    : GOTO IDENTIFIER ';'
    | CONTINUE ';'
    | BREAK ';'
    | RETURN expression_opt ';'
    ;

asm_statement
    : ASM '{' asm_text_opt '}'
    ;

asm_text_opt
    : %empty
    | ASM_TEXT
    ;

pragma_directive
    : PRAGMA_ORG '(' constant_expression ')'
    | PRAGMA_PC '(' constant_expression ')'
    | PRAGMA_NOSAVE
    ;

primary_expression
    : identifier
    | INTEGER_CONSTANT
    | FLOATING_CONSTANT
    | CHARACTER_CONSTANT
    | string_literal_sequence
    | '(' expression ')'
    ;

string_literal_sequence
    : STRING_LITERAL
    | string_literal_sequence STRING_LITERAL
    ;

postfix_expression
    : primary_expression
    | postfix_expression '[' expression ']'
    | postfix_expression '(' argument_expression_list_opt ')'
    | postfix_expression '.' identifier
    | postfix_expression PTR_OP identifier
    | postfix_expression INC_OP
    | postfix_expression DEC_OP
    ;

argument_expression_list_opt
    : %empty
    | argument_expression_list
    ;

argument_expression_list
    : assignment_expression
    | argument_expression_list ',' assignment_expression
    ;

unary_expression
    : postfix_expression
    | INC_OP unary_expression
    | DEC_OP unary_expression
    | unary_operator cast_expression
    | SIZEOF unary_expression
    | SIZEOF '(' type_name ')'
    ;

unary_operator
    : '&'
    | '*'
    | '+'
    | '-'
    | '~'
    | '!'
    ;

cast_expression
    : unary_expression
    | '(' type_name ')' cast_expression
    ;

multiplicative_expression
    : cast_expression
    | multiplicative_expression '*' cast_expression
    | multiplicative_expression '/' cast_expression
    | multiplicative_expression '%' cast_expression
    ;

additive_expression
    : multiplicative_expression
    | additive_expression '+' multiplicative_expression
    | additive_expression '-' multiplicative_expression
    ;

shift_expression
    : additive_expression
    | shift_expression LEFT_OP additive_expression
    | shift_expression RIGHT_OP additive_expression
    ;

relational_expression
    : shift_expression
    | relational_expression '<' shift_expression
    | relational_expression '>' shift_expression
    | relational_expression LE_OP shift_expression
    | relational_expression GE_OP shift_expression
    ;

equality_expression
    : relational_expression
    | equality_expression EQ_OP relational_expression
    | equality_expression NE_OP relational_expression
    ;

and_expression
    : equality_expression
    | and_expression '&' equality_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression '^' and_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression '|' exclusive_or_expression
    ;

logical_and_expression
    : inclusive_or_expression
    | logical_and_expression AND_OP inclusive_or_expression
    ;

logical_or_expression
    : logical_and_expression
    | logical_or_expression OR_OP logical_and_expression
    ;

conditional_expression
    : logical_or_expression
    | logical_or_expression '?' expression ':' conditional_expression
    ;

assignment_expression
    : conditional_expression
    | unary_expression assignment_operator assignment_expression
    ;

assignment_operator
    : '='
    | MUL_ASSIGN
    | DIV_ASSIGN
    | MOD_ASSIGN
    | ADD_ASSIGN
    | SUB_ASSIGN
    | LEFT_ASSIGN
    | RIGHT_ASSIGN
    | AND_ASSIGN
    | XOR_ASSIGN
    | OR_ASSIGN
    ;

expression
    : assignment_expression
    | expression ',' assignment_expression
    ;

type_name
    : specifier_qualifier_list abstract_declarator_opt
    ;

specifier_qualifier_list
    : type_qualifiers_opt type_specifier type_qualifiers_opt
    ;

type_qualifiers_opt
    : %empty
    | type_qualifier_list
    ;

abstract_declarator_opt
    : %empty
    | pointer
    ;

%%

/* Expression precedence is encoded by the layered nonterminals above. */
