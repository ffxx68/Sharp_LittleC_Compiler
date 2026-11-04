%option noyywrap yylineno nodefault noinput nounput never-interactive

%{
#include "lexer_tokens.h"
#include "parser_tokens.h"

#include <stddef.h>
#include <stdio.h>

static unsigned long current_line = 1;
static unsigned long current_column = 1;
static void record_location(const char *text, int length);
int lcc2_parser_is_typedef_name(const char *name, size_t length);

unsigned long lcc2_token_line;
unsigned long lcc2_token_column;

#define YY_USER_ACTION record_location(yytext, yyleng);
#define TEXT_TOKEN(token) do { yylval.text = yytext; return (token); } while (0)
%}

%x COMMENT ASM_BODY_WAIT ASM_BODY

DIGIT       [0-9]
HEX_DIGIT   [0-9a-fA-F]
ID_START    [a-zA-Z_]
ID_CONT     [a-zA-Z0-9_]
EXPONENT    [eE][+-]?{DIGIT}+
INT_SUFFIX  ([uU]([lL][lL]?)?|[lL][lL]?[uU]?)
ESCAPE      \\([\\'"?abfnrtv]|[0-7]{1,3}|x{HEX_DIGIT}+)

%%

[ \t\v\f\r\n]+                 ;
"//"[^\r\n]*                   ;
"/*"                           { BEGIN(COMMENT); }
<COMMENT>"*/"                  { BEGIN(INITIAL); }
<COMMENT>[^*\r\n]+             ;
<COMMENT>"*"                   ;
<COMMENT>(\r\n|\n|\r)          ;

#[ \t]*pragma[ \t]+org/[ \t]*\( { return LCC2_TOK_PRAGMA_ORG; }
#[ \t]*pragma[ \t]+pc/[ \t]*\( { return LCC2_TOK_PRAGMA_PC; }
#[ \t]*pragma[ \t]+nosave/[ \t]*(\r\n|\n|\r|$) { return LCC2_TOK_PRAGMA_NOSAVE; }

"__at"                         { return LCC2_TOK_AT; }
"__xram"                       { return LCC2_TOK_XRAM; }
"__asm"                        { BEGIN(ASM_BODY_WAIT); return LCC2_TOK_ASM; }

"auto"                         { return LCC2_TOK_AUTO; }
"break"                        { return LCC2_TOK_BREAK; }
"case"                         { return LCC2_TOK_CASE; }
"char"                         { return LCC2_TOK_CHAR; }
"const"                        { return LCC2_TOK_CONST; }
"continue"                     { return LCC2_TOK_CONTINUE; }
"default"                      { return LCC2_TOK_DEFAULT; }
"do"                           { return LCC2_TOK_DO; }
"else"                         { return LCC2_TOK_ELSE; }
"enum"                         { return LCC2_TOK_ENUM; }
"extern"                       { return LCC2_TOK_EXTERN; }
"float"                        { return LCC2_TOK_FLOAT; }
"for"                          { return LCC2_TOK_FOR; }
"goto"                         { return LCC2_TOK_GOTO; }
"if"                           { return LCC2_TOK_IF; }
"int"                          { return LCC2_TOK_INT; }
"long"                         { return LCC2_TOK_LONG; }
"register"                     { return LCC2_TOK_REGISTER; }
"return"                       { return LCC2_TOK_RETURN; }
"short"                        { return LCC2_TOK_SHORT; }
"signed"                       { return LCC2_TOK_SIGNED; }
"sizeof"                       { return LCC2_TOK_SIZEOF; }
"static"                       { return LCC2_TOK_STATIC; }
"struct"                       { return LCC2_TOK_STRUCT; }
"switch"                       { return LCC2_TOK_SWITCH; }
"typedef"                      { return LCC2_TOK_TYPEDEF; }
"union"                        { return LCC2_TOK_UNION; }
"unsigned"                     { return LCC2_TOK_UNSIGNED; }
"void"                         { return LCC2_TOK_VOID; }
"volatile"                     { return LCC2_TOK_VOLATILE; }
"while"                        { return LCC2_TOK_WHILE; }

"++"                           { return LCC2_TOK_INC_OP; }
"--"                           { return LCC2_TOK_DEC_OP; }
"->"                           { return LCC2_TOK_PTR_OP; }
"<<"                           { return LCC2_TOK_LEFT_OP; }
">>"                           { return LCC2_TOK_RIGHT_OP; }
"<="                           { return LCC2_TOK_LE_OP; }
">="                           { return LCC2_TOK_GE_OP; }
"=="                           { return LCC2_TOK_EQ_OP; }
"!="                           { return LCC2_TOK_NE_OP; }
"&&"                           { return LCC2_TOK_AND_OP; }
"||"                           { return LCC2_TOK_OR_OP; }
"<<="                          { return LCC2_TOK_LEFT_ASSIGN; }
">>="                          { return LCC2_TOK_RIGHT_ASSIGN; }
"*="                           { return LCC2_TOK_MUL_ASSIGN; }
"/="                           { return LCC2_TOK_DIV_ASSIGN; }
"%="                           { return LCC2_TOK_MOD_ASSIGN; }
"+="                           { return LCC2_TOK_ADD_ASSIGN; }
"-="                           { return LCC2_TOK_SUB_ASSIGN; }
"&="                           { return LCC2_TOK_AND_ASSIGN; }
"^="                           { return LCC2_TOK_XOR_ASSIGN; }
"|="                           { return LCC2_TOK_OR_ASSIGN; }

0[xX]{HEX_DIGIT}+{INT_SUFFIX}? { TEXT_TOKEN(LCC2_TOK_INTEGER_CONSTANT); }
0[bB][01]+{INT_SUFFIX}?        { TEXT_TOKEN(LCC2_TOK_INTEGER_CONSTANT); }
0[0-7]*{INT_SUFFIX}?           { TEXT_TOKEN(LCC2_TOK_INTEGER_CONSTANT); }
[1-9]{DIGIT}*{INT_SUFFIX}?      { TEXT_TOKEN(LCC2_TOK_INTEGER_CONSTANT); }
({DIGIT}+"."{DIGIT}*|"."{DIGIT}+){EXPONENT}?[fFlL]? { TEXT_TOKEN(LCC2_TOK_FLOATING_CONSTANT); }
{DIGIT}+{EXPONENT}[fFlL]?       { TEXT_TOKEN(LCC2_TOK_FLOATING_CONSTANT); }
\'([^\'\\\r\n]|{ESCAPE})+\'     { TEXT_TOKEN(LCC2_TOK_CHARACTER_CONSTANT); }
\"([^\"\\\r\n]|{ESCAPE})*\"    { TEXT_TOKEN(LCC2_TOK_STRING_LITERAL); }

{ID_START}{ID_CONT}*            {
                                    if (lcc2_parser_is_typedef_name(yytext,
                                                                    (size_t)yyleng)) {
                                        TEXT_TOKEN(LCC2_TOK_TYPEDEF_NAME);
                                    }
                                    TEXT_TOKEN(LCC2_TOK_IDENTIFIER);
                                }

<ASM_BODY_WAIT>[ \t\v\f\r\n]+  ;
<ASM_BODY_WAIT>"{"              { BEGIN(ASM_BODY); return '{'; }
<ASM_BODY_WAIT>.                {
                                    fprintf(stderr,
                                            "lexer:%lu:%lu: expected '{' after __asm\n",
                                            lcc2_token_line, lcc2_token_column);
                                    BEGIN(INITIAL);
                                    return LCC2_TOK_LEX_ERROR;
                                }
<ASM_BODY>"}"                   { BEGIN(INITIAL); return '}'; }
<ASM_BODY>[^}]+                 { TEXT_TOKEN(LCC2_TOK_ASM_TEXT); }

"{"|"}"|"("|")"|"["|"]"|";"|","|"."|"?"|":"|"~"|"!"|"+"|"-"|"*"|"/"|"%"|"<"|">"|"="|"&"|"^"|"|" { return (unsigned char)yytext[0]; }

"#"                            {
                                    fprintf(stderr,
                                            "lexer:%lu:%lu: unsupported preprocessor directive\n",
                                            lcc2_token_line, lcc2_token_column);
                                    return LCC2_TOK_LEX_ERROR;
                                }
.                              {
                                    fprintf(stderr,
                                            "lexer:%lu:%lu: invalid character '%s'\n",
                                            lcc2_token_line, lcc2_token_column, yytext);
                                    return LCC2_TOK_LEX_ERROR;
                                }

<COMMENT,ASM_BODY_WAIT,ASM_BODY><<EOF>> {
                                    fprintf(stderr,
                                            "lexer:%lu:%lu: unterminated comment or __asm block\n",
                                            current_line, current_column);
                                    BEGIN(INITIAL);
                                    return LCC2_TOK_LEX_ERROR;
                                }

<<EOF>>                         { return 0; }

%%

static void record_location(const char *text, int length)
{
    int index;

    lcc2_token_line = current_line;
    lcc2_token_column = current_column;
    for (index = 0; index < length; ++index) {
        if (text[index] == '\n' ||
            (text[index] == '\r' &&
             (index + 1 == length || text[index + 1] != '\n'))) {
            ++current_line;
            current_column = 1;
        } else {
            ++current_column;
        }
    }
}

void lcc2_lexer_reset_location(void)
{
    current_line = 1;
    current_column = 1;
    lcc2_token_line = 1;
    lcc2_token_column = 1;
    yylineno = 1;
    BEGIN(INITIAL);
}
