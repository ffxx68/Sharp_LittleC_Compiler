#ifndef LCC2_PARSER_H
#define LCC2_PARSER_H

#include "ast.h"

int lcc2_parse_string(AstArena *arena, const char *source,
                      AstTranslationUnit **translation_unit);

#endif
