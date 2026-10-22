#include "lexer_tokens.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct yy_buffer_state *YY_BUFFER_STATE;

extern YY_BUFFER_STATE yy_scan_string(const char *text);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylex(void);
extern char *yytext;
extern int yyleng;

static void expect_token(int expected, const char *spelling,
                         unsigned long line, unsigned long column)
{
    int token = yylex();

    assert(token == expected);
    if (spelling != NULL) {
        assert((size_t)yyleng == strlen(spelling));
        assert(memcmp(yytext, spelling, (size_t)yyleng) == 0);
    }
    if (lcc2_token_line != line || lcc2_token_column != column) {
        fprintf(stderr, "token %d at %lu:%lu; expected %lu:%lu\n",
                token, lcc2_token_line, lcc2_token_column, line, column);
        assert(lcc2_token_line == line);
        assert(lcc2_token_column == column);
    }
}

int main(void)
{
    YY_BUFFER_STATE buffer;

    buffer = yy_scan_string(
        "auto break case char const continue default do else enum extern "
        "float for goto if int long register return short signed sizeof "
        "static struct switch typedef union unsigned void volatile while "
        "__at __xram __asm { LIA 1\\nLP 8 } "
        "++ -- -> << >> <= >= == != && || <<= >>= *= /= %= += -= &= ^= |= "
        "0 012 123u 0xCAFE 0b101 1.25 .5 1e3 2.0f "
        "'x' '\\n' \"text\" name  ? : { } ;");
    assert(buffer != NULL);

    expect_token(LCC2_TOK_AUTO, "auto", 1, 1);
    expect_token(LCC2_TOK_BREAK, "break", 1, 6);
    expect_token(LCC2_TOK_CASE, "case", 1, 12);
    expect_token(LCC2_TOK_CHAR, "char", 1, 17);
    expect_token(LCC2_TOK_CONST, "const", 1, 22);
    expect_token(LCC2_TOK_CONTINUE, "continue", 1, 28);
    expect_token(LCC2_TOK_DEFAULT, "default", 1, 37);
    expect_token(LCC2_TOK_DO, "do", 1, 45);
    expect_token(LCC2_TOK_ELSE, "else", 1, 48);
    expect_token(LCC2_TOK_ENUM, "enum", 1, 53);
    expect_token(LCC2_TOK_EXTERN, "extern", 1, 58);
    expect_token(LCC2_TOK_FLOAT, "float", 1, 65);
    expect_token(LCC2_TOK_FOR, "for", 1, 71);
    expect_token(LCC2_TOK_GOTO, "goto", 1, 75);
    expect_token(LCC2_TOK_IF, "if", 1, 80);
    expect_token(LCC2_TOK_INT, "int", 1, 83);
    expect_token(LCC2_TOK_LONG, "long", 1, 87);
    expect_token(LCC2_TOK_REGISTER, "register", 1, 92);
    expect_token(LCC2_TOK_RETURN, "return", 1, 101);
    expect_token(LCC2_TOK_SHORT, "short", 1, 108);
    expect_token(LCC2_TOK_SIGNED, "signed", 1, 114);
    expect_token(LCC2_TOK_SIZEOF, "sizeof", 1, 121);
    expect_token(LCC2_TOK_STATIC, "static", 1, 128);
    expect_token(LCC2_TOK_STRUCT, "struct", 1, 135);
    expect_token(LCC2_TOK_SWITCH, "switch", 1, 142);
    expect_token(LCC2_TOK_TYPEDEF, "typedef", 1, 149);
    expect_token(LCC2_TOK_UNION, "union", 1, 157);
    expect_token(LCC2_TOK_UNSIGNED, "unsigned", 1, 163);
    expect_token(LCC2_TOK_VOID, "void", 1, 172);
    expect_token(LCC2_TOK_VOLATILE, "volatile", 1, 177);
    expect_token(LCC2_TOK_WHILE, "while", 1, 186);
    expect_token(LCC2_TOK_AT, "__at", 1, 192);
    expect_token(LCC2_TOK_XRAM, "__xram", 1, 197);
    expect_token(LCC2_TOK_ASM, "__asm", 1, 204);
    expect_token('{', "{", 1, 210);
    expect_token(LCC2_TOK_ASM_TEXT, " LIA 1\\nLP 8 ", 1, 211);
    expect_token('}', "}", 1, 224);

    expect_token(LCC2_TOK_INC_OP, "++", 1, 226);
    expect_token(LCC2_TOK_DEC_OP, "--", 1, 229);
    expect_token(LCC2_TOK_PTR_OP, "->", 1, 232);
    expect_token(LCC2_TOK_LEFT_OP, "<<", 1, 235);
    expect_token(LCC2_TOK_RIGHT_OP, ">>", 1, 238);
    expect_token(LCC2_TOK_LE_OP, "<=", 1, 241);
    expect_token(LCC2_TOK_GE_OP, ">=", 1, 244);
    expect_token(LCC2_TOK_EQ_OP, "==", 1, 247);
    expect_token(LCC2_TOK_NE_OP, "!=", 1, 250);
    expect_token(LCC2_TOK_AND_OP, "&&", 1, 253);
    expect_token(LCC2_TOK_OR_OP, "||", 1, 256);
    expect_token(LCC2_TOK_LEFT_ASSIGN, "<<=", 1, 259);
    expect_token(LCC2_TOK_RIGHT_ASSIGN, ">>=", 1, 263);
    expect_token(LCC2_TOK_MUL_ASSIGN, "*=", 1, 267);
    expect_token(LCC2_TOK_DIV_ASSIGN, "/=", 1, 270);
    expect_token(LCC2_TOK_MOD_ASSIGN, "%=", 1, 273);
    expect_token(LCC2_TOK_ADD_ASSIGN, "+=", 1, 276);
    expect_token(LCC2_TOK_SUB_ASSIGN, "-=", 1, 279);
    expect_token(LCC2_TOK_AND_ASSIGN, "&=", 1, 282);
    expect_token(LCC2_TOK_XOR_ASSIGN, "^=", 1, 285);
    expect_token(LCC2_TOK_OR_ASSIGN, "|=", 1, 288);

    expect_token(LCC2_TOK_INTEGER_CONSTANT, "0", 1, 291);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "012", 1, 293);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "123u", 1, 297);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "0xCAFE", 1, 302);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "0b101", 1, 309);
    expect_token(LCC2_TOK_FLOATING_CONSTANT, "1.25", 1, 315);
    expect_token(LCC2_TOK_FLOATING_CONSTANT, ".5", 1, 320);
    expect_token(LCC2_TOK_FLOATING_CONSTANT, "1e3", 1, 323);
    expect_token(LCC2_TOK_FLOATING_CONSTANT, "2.0f", 1, 327);
    expect_token(LCC2_TOK_CHARACTER_CONSTANT, "'x'", 1, 332);
    expect_token(LCC2_TOK_CHARACTER_CONSTANT, "'\\n'", 1, 336);
    expect_token(LCC2_TOK_STRING_LITERAL, "\"text\"", 1, 341);
    expect_token(LCC2_TOK_IDENTIFIER, "name", 1, 348);
    expect_token('?', "?", 1, 354);
    expect_token(':', ":", 1, 356);
    expect_token('{', "{", 1, 358);
    expect_token('}', "}", 1, 360);
    expect_token(';', ";", 1, 362);
    assert(yylex() == 0);

    yy_delete_buffer(buffer);

    buffer = yy_scan_string(
        " \n/* first line\n second line */\n"
        "  #pragma org(33000)\n"
        "#pragma pc(33000)\n"
        "#pragma nosave\n");
    assert(buffer != NULL);
    lcc2_lexer_reset_location();
    expect_token(LCC2_TOK_PRAGMA_ORG, "#pragma org", 4, 3);
    expect_token('(', "(", 4, 14);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "33000", 4, 15);
    expect_token(')', ")", 4, 20);
    expect_token(LCC2_TOK_PRAGMA_PC, "#pragma pc", 5, 1);
    expect_token('(', "(", 5, 11);
    expect_token(LCC2_TOK_INTEGER_CONSTANT, "33000", 5, 12);
    expect_token(')', ")", 5, 17);
    expect_token(LCC2_TOK_PRAGMA_NOSAVE, "#pragma nosave", 6, 1);
    assert(yylex() == 0);
    yy_delete_buffer(buffer);

    buffer = yy_scan_string(" @");
    assert(buffer != NULL);
    lcc2_lexer_reset_location();
    expect_token(LCC2_TOK_LEX_ERROR, "@", 1, 2);
    assert(yylex() == 0);
    yy_delete_buffer(buffer);

    buffer = yy_scan_string("/*");
    assert(buffer != NULL);
    lcc2_lexer_reset_location();
    expect_token(LCC2_TOK_LEX_ERROR, NULL, 1, 1);
    assert(yylex() == 0);
    yy_delete_buffer(buffer);

    buffer = yy_scan_string("__asm {");
    assert(buffer != NULL);
    lcc2_lexer_reset_location();
    expect_token(LCC2_TOK_ASM, "__asm", 1, 1);
    expect_token('{', "{", 1, 7);
    expect_token(LCC2_TOK_LEX_ERROR, NULL, 1, 7);
    assert(yylex() == 0);
    yy_delete_buffer(buffer);
    return 0;
}
