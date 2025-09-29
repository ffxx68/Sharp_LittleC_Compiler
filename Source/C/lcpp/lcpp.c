/*
 * lcpp v2.0 - preprocessor for lcc
 * (c) Simon Lehmayr 2004
 * Migrated to C for GCC - Enhanced Version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SYMBOLS 1000
#define MAX_LINE_LEN 2048
#define MAX_FILENAME_LEN 256

static int cline = 0;
static FILE *fout = NULL;
static char cf[MAX_FILENAME_LEN];
static int symcnt = 0;
static char sym[MAX_SYMBOLS][256];
static char symval[MAX_SYMBOLS][256];


void abort_with_error(const char *text) {
    fprintf(stderr, "Line %d: %s in file %s\n", cline + 1, text, cf);
    exit(1);
}

int file_exists(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

char *trim(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    end[1] = '\0';

    return str;
}

char *replace_text(const char *text, const char *such, const char *ers) {
    static char result[MAX_LINE_LEN * 2];
    char work[MAX_LINE_LEN * 2];
    char c = ' ';
    int i, such_len, ers_len, work_len;

    if (!such || !text || strlen(such) == 0 || strlen(text) == 0) {
        strcpy(result, text);
        return result;
    }

    // Add spaces at beginning and end for word boundary checking
    snprintf(work, sizeof(work), " %s ", text);
    such_len = strlen(such);
    ers_len = strlen(ers);
    work_len = (int)strlen(work);

    i = 1;
    while (i < work_len) {
        // Track string literals
        if (work[i] == '\'' || work[i] == '"') {
            if (c == ' ')
                c = work[i];
            else
                c = ' ';
        }

        // Only replace outside string literals
        if (c == ' ' && strncmp(&work[i], such, such_len) == 0) {
            // Check word boundaries
            char prev = (i > 0) ? work[i-1] : ' ';
            char next = (i + such_len < work_len) ? work[i + such_len] : ' ';

            if (!((isalnum(prev) || prev == '_') || (isalnum(next) || next == '_'))) {
                // Replace the text
                memmove(&work[i + ers_len], &work[i + such_len], strlen(&work[i + such_len]) + 1);
                memcpy(&work[i], ers, ers_len);
                i += ers_len;
                work_len = (int)strlen(work); // Update work_len after modification
                continue;
            }
        }
        i++;
    }

    strcpy(result, trim(work));
    return result;
}

int find_symbol(const char *name) {
    for (int i = 0; i < symcnt; i++) {
        if (strcmp(sym[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

char *normalize_spaces(const char *text) {
    static char result[MAX_LINE_LEN];
    char c = ' ';
    int in_string = 0;
    int result_pos = 0;
    char prev_char = ' ';
    int text_len;

    if (!text || strlen(text) == 0) {
        result[0] = '\0';
        return result;
    }

    text_len = (int)strlen(text);
    for (int i = 0; i < text_len; i++) {
        char curr_char = text[i];

        // Track string literal boundaries
        if (curr_char == '\'' || curr_char == '"') {
            if (c == ' ')
                c = curr_char;
            else
                c = ' ';
            in_string = (c != ' ');
        }

        if (in_string) {
            // Inside string literals, preserve everything
            result[result_pos++] = curr_char;
        } else if (curr_char == ' ') {
            // Only add space if needed between alphanumeric/underscore characters
            int need_space = (isalnum(prev_char) || prev_char == '_') &&
                           (i + 1 < text_len) &&
                           (isalnum(text[i+1]) || text[i+1] == '_');
            if (need_space) {
                result[result_pos++] = ' ';
            }
        } else {
            // Non-space character, always add
            result[result_pos++] = curr_char;
        }

        if (curr_char != ' ') {
            prev_char = curr_char;
        }
    }

    result[result_pos] = '\0';
    return trim(result);
}

void add_symbol(const char *s1, const char *s2) {
    if (find_symbol(s1)) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Symbol %s already defined!", s1);
        abort_with_error(error_msg);
    }

    if (symcnt >= MAX_SYMBOLS) {
        abort_with_error("Too many symbols defined");
    }

    strcpy(sym[symcnt], s1);
    strcpy(symval[symcnt], s2);
    printf("SYMBOL: %s - %s\n", s1, s2);
    symcnt++;
}

char *extract_param(const char *s, int p) {
    static char result1[256];
    static char result2[256];
    static char result3[256];
    static int last_call = 0;
    char work[MAX_LINE_LEN];
    char *result;
    int pos = 0;
    int param_count = 0;
    int paren_depth = 0;
    int in_quotes = 0;
    char quote_char = 0;

    // Use different static buffers to avoid conflicts
    switch (last_call % 3) {
        case 0: result = result1; break;
        case 1: result = result2; break;
        case 2: result = result3; break;
        default: result = result1; break;
    }
    last_call++;

    strcpy(work, s);

    // Skip whitespace at beginning
    while (work[pos] && (work[pos] == ' ' || work[pos] == '\t')) pos++;

    // Skip the directive (#define, #ifdef, etc.)
    while (work[pos] && work[pos] != ' ' && work[pos] != '\t') pos++;

    // Skip whitespace after directive
    while (work[pos] && (work[pos] == ' ' || work[pos] == '\t')) pos++;

    // Now extract parameters
    int param_start = pos;

    while (work[pos] && param_count < p) {
        char c = work[pos];

        // Handle quotes
        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = 1;
            quote_char = c;
        } else if (c == quote_char && in_quotes) {
            in_quotes = 0;
            quote_char = 0;
        }

        // Handle parentheses (only count when not in quotes)
        if (!in_quotes) {
            if (c == '(') paren_depth++;
            else if (c == ')') paren_depth--;
        }

        // Check for parameter boundary (space/tab when not in quotes or parentheses)
        if (!in_quotes && paren_depth == 0 && (c == ' ' || c == '\t')) {
            param_count++;
            if (param_count == p) {
                // Found our parameter, extract it
                int len = pos - param_start;
                if (len > 0 && len < 255) {
                    strncpy(result, &work[param_start], len);
                    result[len] = '\0';
                    return trim(result);
                }
            }

            // Skip to next parameter
            while (work[pos] && (work[pos] == ' ' || work[pos] == '\t')) pos++;
            param_start = pos;
            continue;
        }

        pos++;
    }

    // Handle last parameter (if we're at end of string)
    if (param_count + 1 == p && param_start < pos) {
        int len = pos - param_start;
        if (len > 0 && len < 255) {
            strncpy(result, &work[param_start], len);
            result[len] = '\0';
            return trim(result);
        }
    }

    // If we didn't find the parameter, return everything from param_start to end
    if (param_count + 1 == p && work[param_start]) {
        strcpy(result, &work[param_start]);
        return trim(result);
    }

    result[0] = '\0';
    return result;
}

char *read_line(FILE *datei, int *lcnt, int *lcom) {
    static char result[MAX_LINE_LEN];
    char *pos;
    char c = ' ';
    int i;
    int result_len;

    do {
        do {
            if (fgets(result, sizeof(result), datei) == NULL) {
                if (feof(datei)) {
                    result[0] = '\0';
                    return result;
                }
            }
            (*lcnt)++;
            cline = *lcnt;

            // Remove newline
            if ((pos = strchr(result, '\n')) != NULL) {
                *pos = '\0';
            }
            if ((pos = strchr(result, '\r')) != NULL) {
                *pos = '\0';
            }

            strcpy(result, trim(result));

            // Handle multi-line comments
            if (*lcom) {
                if ((pos = strstr(result, "*/")) != NULL) {
                    memmove(result, pos + 2, strlen(pos + 2) + 1);
                    *lcom = 0;
                } else {
                    result[0] = '\0';
                }
            }
        } while (!feof(datei) && strlen(result) == 0);

        if (feof(datei) && strlen(result) == 0) {
            return result;
        }

        // Handle single-line and inline comments
        c = ' ';
        result_len = (int)strlen(result);
        for (i = 0; i < result_len - 1; i++) {
            if (result[i] == '\'' || result[i] == '"') {
                if (c == ' ')
                    c = result[i];
                else
                    c = ' ';
            }

            if (c == ' ') {
                if (strncmp(&result[i], "//", 2) == 0) {
                    result[i] = '\0';
                    break;
                }
                if (strncmp(&result[i], "/*", 2) == 0) {
                    char *end = strstr(&result[i], "*/");
                    if (end) {
                        memmove(&result[i], end + 2, strlen(end + 2) + 1);
                    } else {
                        result[i] = '\0';
                        *lcom = 1;
                    }
                    break;
                }
            }
        }

        if (strlen(result) > 0) {
            // Replace symbols, but not in #ifdef lines
            if (strncmp(result, "#ifdef", 6) != 0) {
                for (i = 0; i < symcnt; i++) {
                    char *replaced = replace_text(result, sym[i], symval[i]);
                    strcpy(result, replaced);
                }
            }
        }
    } while (!feof(datei) && strlen(result) == 0);

    return trim(result);
}

void parse_file(const char *fname) {
    FILE *datei;
    int lcnt = 0;
    int lcom = 0;
    char *tok;
    char old_cf[MAX_FILENAME_LEN];

    if (!file_exists(fname)) {
        printf("File %s not found!\n", fname);
        return;
    }

    datei = fopen(fname, "r");
    if (!datei) {
        printf("Cannot open file %s\n", fname);
        return;
    }

    // Save old filename and set new one
    strcpy(old_cf, cf);
    strcpy(cf, fname);

    tok = read_line(datei, &lcnt, &lcom);
    while (!feof(datei) || strlen(tok) > 0) {
        if (strncmp(tok, "#define", 7) == 0) {
            char *param1 = extract_param(tok, 1);
            char *param2 = extract_param(tok, 2);
            add_symbol(param1, param2);
        } else if (strncmp(tok, "#org", 4) == 0 ||
                   strncmp(tok, "#asm", 4) == 0 ||
                   strncmp(tok, "#endasm", 7) == 0 ||
                   strncmp(tok, "#nosave", 7) == 0) {
            fprintf(fout, "%s;\n", tok);
        } else if (strncmp(tok, "#ifdef", 6) == 0) {
            char *param = extract_param(tok, 1);
            if (!find_symbol(param)) {
                // Skip until #endif
                do {
                    tok = read_line(datei, &lcnt, &lcom);
                } while (!feof(datei) && strncmp(tok, "#endif", 6) != 0);
            }
        } else if (strncmp(tok, "#include", 8) == 0) {
            char *include_file = extract_param(tok, 1);
            if (file_exists(include_file)) {
                parse_file(include_file);
            } else {
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Include file %s not found!", include_file);
                abort_with_error(error_msg);
            }
        } else if (strncmp(tok, "#endif", 6) != 0) {
            fprintf(fout, "%s\n", normalize_spaces(tok));
        }

        if (!feof(datei) || strlen(tok) > 0) {
            tok = read_line(datei, &lcnt, &lcom);
        } else {
            break;
        }
    }

    fclose(datei);

    // Restore old filename
    strcpy(cf, old_cf);
}

int main(int argc, char *argv[]) {
    printf("lcpp v2.0 - preprocessor for lcc\n");
    printf("(c) Simon Lehmayr 2004 - Enhanced C Version\n");

    if (argc < 3) {
        printf("Usage: lcpp cfile [cfile]* outputfile\n");
        printf("       You can enter multiple c files\n");
        return 1;
    }

    fout = fopen(argv[argc - 1], "w");
    if (!fout) {
        printf("Cannot create output file %s\n", argv[argc - 1]);
        return 1;
    }

    for (int i = 1; i < argc - 1; i++) {
        parse_file(argv[i]);
    }

    fprintf(fout, ";");
    fclose(fout);

    return 0;
}
