// pasm.c
// File generated for migration from Pascal to C
// Data structures and initial constants

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "pasm_constants.h"
#include <stdarg.h>

// Provide fallback implementations for strcasecmp/strncasecmp if missing
#ifndef HAVE_STRCASECMP
static int my_strcasecmp(const char *a, const char *b) {
    unsigned char ca, cb;
    while (*a && *b) {
        ca = (unsigned char) tolower((unsigned char)*a);
        cb = (unsigned char) tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    ca = (unsigned char) tolower((unsigned char)*a);
    cb = (unsigned char) tolower((unsigned char)*b);
    return (int)ca - (int)cb;
}
static int my_strncasecmp(const char *a, const char *b, size_t n) {
    unsigned char ca, cb;
    size_t i = 0;
    for (; i < n && *a && *b; i++) {
        ca = (unsigned char) tolower((unsigned char)*a);
        cb = (unsigned char) tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    if (i == n) return 0;
    ca = (unsigned char) tolower((unsigned char)*a);
    cb = (unsigned char) tolower((unsigned char)*b);
    return (int)ca - (int)cb;
}
// Map names used in code to the fallback implementations
#define strcasecmp my_strcasecmp
#define strncasecmp my_strncasecmp
#endif

#define CODE_SIZE 100001
#define SYM_SIZE 1001
#define LAB_SIZE 1001
#define NLAB_SIZE 1001

// Forward declarations
void doasm(const char *line);
void extractop(const char *src);
int mathparse(const char *expr, int base);
int calcadr();
int parse_expr(const char **p); // aggiunta dichiarazione

// Global variables
char tok[256], op[256], param1[256], param2[256], params[256];
unsigned char code[CODE_SIZE];
int codpos = 0;
char sym[SYM_SIZE][256];
char symval[SYM_SIZE][256];
int symcnt = 0;
char lab[LAB_SIZE][256];
int labpos[LAB_SIZE];
int labbase[LAB_SIZE];
int labcnt = 0, labp = 0;
char nlab[NLAB_SIZE][256];
int nlabpos[NLAB_SIZE];
char nlabasm[NLAB_SIZE][256];
int nlabcnt = 0, nlabp = 0;
int startadr = 0, blcnt, opp = 0;
bool mcase = false;
int ccase = 0, casecnt = 0, cline = 0, i = 0;
char s[1024], cf[256];

// Global handle for debug file and control flag
FILE *debug_file = NULL;
bool debug_enabled = false;

// Function to open the debug file
void open_debug_file() {
    if (debug_enabled && !debug_file) {
        debug_file = fopen("debug.txt", "w");
    }
}

// Function to write to the debug file
void write_debug(const char *format, ...) {
    if (debug_file) {
        va_list args;
        va_start(args, format);
        // Print the source line number before the message
        fprintf(debug_file, "[Line %d] ", cline);
        vfprintf(debug_file, format, args);
        va_end(args);
        fflush(debug_file);
    }
}

// Function to close the debug file
void close_debug_file() {
    if (debug_file) {
        fclose(debug_file);
        debug_file = NULL;
    }
}

// Utility: trim (remove spaces, tabs, newline)
void trim(char *str) {
    char *end;
    char *start = str;

    // Find the start of non-whitespace characters
    while(*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;

    // If the string is all whitespace
    if(*start == 0) {
        str[0] = 0;
        return;
    }

    // Move characters to the beginning of the string if necessary
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    // Trim trailing spaces
    end = str + strlen(str) - 1;
    while(end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end+1) = 0;
}

// Utility: replace_text (safe copy limit)
void replace_text(char *text, const char *such, const char *ers) {
    if (!text || !such || strlen(such) == 0 || strlen(text) == 0) return;
    char work[1024];
    char lower_work[1024];
    char lower_such[256];
    size_t such_len = strlen(such);

    // Build work = ' ' + text + ' '
    snprintf(work, sizeof(work), " %s ", text);
    work[sizeof(work)-1] = '\0';

    // Lowercase copies for case-insensitive matching
    for (size_t i = 0; work[i] && i < sizeof(lower_work)-1; i++) lower_work[i] = tolower((unsigned char)work[i]);
    lower_work[strlen(work)] = '\0';

    for (size_t i = 0; i < such_len && i < sizeof(lower_such)-1; i++) lower_such[i] = tolower((unsigned char)such[i]);
    lower_such[such_len < sizeof(lower_such) ? such_len : sizeof(lower_such)-1] = '\0';

    char buffer[1024];
    buffer[0] = '\0';

    int wlen = (int)strlen(work);
    int i = 1; // start after leading space to mimic Pascal behavior
    int last = 0;
    while (i <= wlen - (int)such_len - 1) {
        if (strncmp(lower_work + i, lower_such, such_len) == 0) {
            // check boundaries: previous and next chars must NOT be alnum or underscore
            char prev = lower_work[i-1];
            char next = lower_work[i + such_len];
            bool prev_ok = !(prev == '_' || isalnum((unsigned char)prev));
            bool next_ok = !(next == '_' || isalnum((unsigned char)next));
            if (prev_ok && next_ok) {
                // append original substring from last to i
                int chunk_len = i - last;
                if ((int)strlen(buffer) + chunk_len + (int)strlen(ers) < (int)sizeof(buffer)-1) {
                    strncat(buffer, work + last, chunk_len);
                    strcat(buffer, ers);
                }
                i += (int)such_len;
                last = i;
                continue;
            }
        }
        i++;
    }
    // append the remainder
    if ((int)strlen(buffer) + (wlen - last) < (int)sizeof(buffer)-1) {
        strncat(buffer, work + last, wlen - last);
    }

    // remove the added leading/trailing space and copy back to text safely
    // buffer may still have leading space
    char *start = buffer;
    if (buffer[0] == ' ') start = buffer + 1;
    // trim trailing space
    size_t blen = strlen(start);
    if (blen > 0 && start[blen-1] == ' ') ((char*)start)[blen-1] = '\0';

    // copy back to text with safe limit
    snprintf(text, 256, "%s", start);
    text[255] = '\0';
    trim(text);
}

// Utility: abort
void abort_c(const char *t) {
    fprintf(stderr, "Line %d: %s in file %s\n", cline + 1, t, cf);
    exit(1);
}

// Utility: findnlabel
bool findnlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    nlabp = -1;
    for (i = 0; i < nlabcnt; i++) {
        if (strcmp(nlab[i], up) == 0) {
            nlabp = i;
            return true;
        }
    }
    return false;
}

// Utility: addnlabel
void addnlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    // Validate label name: must start with letter or underscore and contain only alnum or underscore
    if (!(isalpha((unsigned char)up[0]) || up[0] == '_')) return;
    for (i = 1; up[i]; i++) {
        if (!(isalnum((unsigned char)up[i]) || up[i] == '_')) return;
    }
    strcpy(nlab[nlabcnt], up);
    nlabpos[nlabcnt] = codpos;
    snprintf(nlabasm[nlabcnt], 255, "%s %s", op, params);
    if (mcase) {
        char tmp[256];
        snprintf(tmp, 255, "#%s %s", op, params);
        strncpy(nlabasm[nlabcnt], tmp, 255);
    }
    nlabcnt++;
}

// Utility: delnlabel
void delnlabel(int l) {
    int i;
    for (i = l; i < nlabcnt - 1; i++) {
        strcpy(nlab[i], nlab[i + 1]);
        nlabpos[i] = nlabpos[i + 1];
        strcpy(nlabasm[i], nlabasm[i + 1]);
    }
    nlabcnt--;
}

// Utility: findlabel
bool findlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    labp = -1;
    for (i = 0; i < labcnt; i++) {
        if (strcmp(lab[i], up) == 0) {
            labp = i;
            return true;
        }
    }
    return false;
}

// Utility: addlabel (partial)
void addsymbol(const char *s1, const char *s2); // dichiarazione spostata qui
void addlabel(const char *l) {
    char up[256];
    int tpos, bup;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (int i = 0; up[i]; i++) up[i] = toupper(up[i]);
    if (findlabel(up)) abort_c("Label already defined!");
    strcpy(lab[labcnt], up);
    labpos[labcnt] = codpos;
    labbase[labcnt] = startadr; // store origin at definition time
    labcnt++;
    // Registrazione anche come simbolo
    char val_str[32];
    snprintf(val_str, 31, "%d", startadr + codpos);
    addsymbol(up, val_str);

    // Replicate Pascal behavior: if any nlab matches this label, reassemble them now
    while (findnlabel(up)) {
        // Save current state
        int saved_codpos = codpos;
        bool saved_mcase = mcase;
        char saved_op[256], saved_params[256], saved_param1[256], saved_param2[256];
        strcpy(saved_op, op);
        strcpy(saved_params, params);
        strcpy(saved_param1, param1);
        strcpy(saved_param2, param2);

        // Reposition code to the unresolved label position and reassemble
        codpos = nlabpos[nlabp];
        char tokbuf[512];
        strncpy(tokbuf, nlabasm[nlabp], 511);
        tokbuf[511] = '\0';
        if (tokbuf[0] == '#') {
            mcase = true;
            // remove leading '#'
            memmove(tokbuf, tokbuf + 1, strlen(tokbuf));
        }
        // extractop and doasm will use global op/params
        extractop(tokbuf);
        doasm(tokbuf);

        // Restore state
        codpos = saved_codpos;
        mcase = saved_mcase;
        strcpy(op, saved_op);
        strcpy(params, saved_params);
        strcpy(param1, saved_param1);
        strcpy(param2, saved_param2);

        // Remove the resolved nlab entry
        delnlabel(nlabp);
    }
}

// Utility: findsymbol
bool findsymbol(const char *l) {
    int i;
    for (i = 0; i < symcnt; i++) {
        if (strcmp(sym[i], l) == 0) {
            return true;
        }
    }
    return false;
}

// Utility: addsymbol (ensure defined for .equ)
void addsymbol(const char *s1, const char *s2) {
    if (findsymbol(s1)) abort_c("Symbol already defined!");
    // store symbol name uppercase for consistent lookup
    char up[256]; strncpy(up, s1, 255); up[255] = '\0';
    for (int k = 0; up[k]; k++) up[k] = toupper((unsigned char)up[k]);
    strncpy(sym[symcnt], up, 255); sym[symcnt][255] = '\0';
    strncpy(symval[symcnt], s2, 255); symval[symcnt][255] = '\0';
    //printf("SYMBOL: %s - %s\n", sym[symcnt], s2);
    symcnt++;
}

// Utility: findop
bool findop(const char *l) {
    int i;
    opp = -1;
    for (i = 0; i < 256; i++) {
        if (strcmp(OPCODE[i], l) == 0) {
            opp = i;
            if (debug_enabled) {
                write_debug("findop match: op='%s' index=%d\n", l, i);
            }
            return true;
        }
    }
    if (debug_enabled) {
        write_debug("findop NO match: op='%s'\n", l);
    }
    return false;
}

// Utility: addcode
void addcode(unsigned char b) {
    if (debug_enabled) {
        write_debug("addcode b=%d codpos=%d\n", b, codpos);
    }
    int idx = codpos;
    code[idx] = (unsigned char)(b & 0xFF);
    codpos++;
    if (debug_enabled) {
        write_debug("wrote code[%d]=%02X\n", idx, code[idx]);
    }
    if (codpos + startadr >= 65536) {
        abort_c("Code exceeds maximum memory!");
    }
}

// Utility: extractop
void extractop(const char *src) {
    char s[1024];
    int i = 0, j = 0;
    if (src == NULL || src[0] == '\0') return;
    strncpy(s, src, 1023);
    s[1023] = '\0';
    strcat(s, " ");
    // Skip initial spaces
    while (s[i] == ' ' || s[i] == '\t') i++;
    // Copy the operator
    j = 0;
    while (s[i] != ' ' && s[i] != '\t' && s[i] != '\0') {
        op[j++] = toupper(s[i++]);
    }
    op[j] = '\0';
    // Skip spaces after operator
    while (s[i] == ' ' || s[i] == '\t') i++;
    // Copy the parameters
    strncpy(params, s + i, 255);
    params[255] = '\0';
    // Truncate params at inline comment ';' if present
    char *pcomment = strchr(params, ';');
    if (pcomment) *pcomment = '\0';
    // Replace the first comma with space
    char *comma = strchr(params, ',');
    if (comma) *comma = ' ';
    // Extract param1 and param2
    i = 0;
    while (params[i] == ' ' || params[i] == '\t') i++;
    j = 0;
    while (params[i] != ' ' && params[i] != '\t' && params[i] != '\0') {
        param1[j++] = params[i++];
    }
    param1[j] = '\0';
    while (params[i] == ' ' || params[i] == '\t') i++;
    if (params[i] != '\0') {
        strncpy(param2, params + i, 255);
        param2[255] = '\0';
    } else {
        param2[0] = '\0';
    }
    // Clean residual comments and whitespace from parameters
    char *c;
    c = strchr(param1, ';'); if (c) *c = '\0';
    c = strchr(param2, ';'); if (c) *c = '\0';
    trim(param1);
    trim(param2);
    if (param1[0] == ';' || param1[0] == '\0') param1[0] = '\0';
}

// Utility: calcadr improved to replicate Pascal logic
int calcadr() {
    int i = 0;
    char s[256], s2[256];
    bool lf = false;
    int len = strlen(params);
    // Se il parametro è già numerico, non cercare label
    char *endptr = NULL;
    long num = strtol(params, &endptr, 0);
    if (endptr != params && *endptr == '\0') {
        // Parametro numerico valido
        strcpy(param1, params);
        param2[0] = '\0';
        return (int)num;
    }
    // Altrimenti, cerca solo token interi separati da delimitatori
    while (i < len) {
        // Skip hexadecimal numbers (0x...)
        if (i < len - 2 && params[i] == '0' && (params[i+1] == 'x' || params[i+1] == 'X')) {
            i += 2;
            while (i < len && (isdigit(params[i]) || (toupper(params[i]) >= 'A' && toupper(params[i]) <= 'F'))) i++;
        }
        // Skip binary numbers (0b...)
        else if (i < len - 2 && params[i] == '0' && (params[i+1] == 'b' || params[i+1] == 'B')) {
            i += 2;
            while (i < len && (params[i] == '0' || params[i] == '1')) i++;
        }
        // Extract token (label or symbol) solo se è un token intero e case-sensitive
        else if (isalpha(params[i]) || params[i] == '_') {
            int j = 0;
            s[0] = '\0';
            int start = i;
            while (i < len && (isalnum(params[i]) || params[i] == '_')) {
                s[j++] = params[i];
                i++;
            }
            s[j] = '\0';
            // Verifica che il token sia delimitato (fine stringa o delimitatore)
            if (i == len || params[i] == ' ' || params[i] == ',' || params[i] == ';' || params[i] == ')' || params[i] == '(') {
                // Se il token non è un valore numerico, cerca label
                char *endptr2 = NULL;
                long num2 = strtol(s, &endptr2, 0);
                if (!(endptr2 != s && *endptr2 == '\0')) {
                    // Check if token is just a suffix of an existing defined label; if so, skip adding it as unresolved
                    char up_s[256]; int ui=0;
                    for (int ti=0; s[ti] && ui < 255; ti++) up_s[ui++] = toupper((unsigned char)s[ti]);
                    up_s[ui] = '\0';
                    bool is_suffix_of_defined = false;
                    for (int li = 0; li < labcnt; li++) {
                        if (strstr(lab[li], up_s) != NULL && strcmp(lab[li], up_s) != 0) {
                            is_suffix_of_defined = true;
                            break;
                        }
                    }
                    if (!is_suffix_of_defined) {
                        if (!findlabel(s)) {
                            // Aggiungi label solo se non è numerica
                            addnlabel(s);
                            lf = true;
                        } else {
                            // Replace the label with il suo valore assoluto
                            snprintf(s2, 255, "%d", labbase[labp] + labpos[labp]);
                            replace_text(params, s, s2);
                            strcpy(param1, params);
                            param2[0] = '\0';
                            i = start + strlen(s2);
                        }
                    } else {
                        // skip this token because it's likely a fragment of a defined label
                    }
                }
            }
        } else {
            i++;
        }
    }
    // For CALL/JP, force param1 to be the resolved label value if present
    // This ensures the correct address is used in the encoding
    if (opp == 120 || opp == 121) {
        for (int k = 0; k < labcnt; k++) {
            if (strstr(params, lab[k]) != NULL) {
                snprintf(param1, 255, "%d", labbase[k] + labpos[k]);
                param2[0] = '\0';
                break;
            }
        }
    }
    if (lf) {
        return 0;  // unresolved label
    } else if (param2[0] == '\0') {
        return mathparse(param1, 10);
    } else {
        return mathparse(param1, 8) * 256 + mathparse(param2, 8);
    }
}


// Utility: in_set
bool in_set(int value, const int *set, int size) {
    for (int i = 0; i < size; i++) {
        if (set[i] == value) return true;
    }
    return false;
}

// Utility: parse and evaluate simple expressions (supports +, -, parentheses, symbols, hex 0x, bin 0b, decimal)
static int parse_number(const char **sp) {
    const char *s = *sp;
    while (*s == ' ' || *s == '\t') s++;
    // LB(x) e HB(x) supporto
    if (strncmp(s, "LB(", 3) == 0) {
        s += 3;
        int v = parse_expr(&s);
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ')') s++;
        *sp = s;
        return v & 0xFF;
    }
    if (strncmp(s, "HB(", 3) == 0) {
        s += 3;
        int v = parse_expr(&s);
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ')') s++;
        *sp = s;
        return (v >> 8) & 0xFF;
    }
    // symbol check
    char tok[256]; int ti=0;
    if (isalpha((unsigned char)*s) || *s == '_') {
        while (isalnum((unsigned char)*s) || *s == '_') {
            if (ti < 255) tok[ti++] = toupper((unsigned char)*s);
            s++;
        }
        tok[ti] = '\0';
        // lookup symbol
        for (int i=0;i<symcnt;i++) if (strcmp(sym[i], tok)==0) {
            *sp = s; return atoi(symval[i]);
        }
        // if not found, return 0
        *sp = s; return 0;
    }
    // parentheses
    if (*s == '(') {
        s++;
        *sp = s;
        int v = 0;
        // call expr parser below by setting pointer
        // we'll use parse_expr which accepts const char**
        // forward declare
        extern int parse_expr(const char **p);
        v = parse_expr(&s);
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ')') s++; // consume
        *sp = s; return v;
    }
    // hex
    if (s[0]=='0' && (s[1]=='x' || s[1]=='X')) {
        s += 2;
        long val = strtol(s, (char**)&s, 16);
        *sp = s; return (int)val;
    }
    // bin
    if (s[0]=='0' && (s[1]=='b' || s[1]=='B')) {
        s += 2;
        int val=0;
        while(*s=='0' || *s=='1') { val = (val<<1) + (*s - '0'); s++; }
        *sp = s; return val;
    }
    // decimal
    long val = strtol(s, (char**)&s, 10);
    *sp = s; return (int)val;
}

// forward declare to allow recursion
int parse_expr(const char **p) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t') s++;
    int value = parse_number(&s);
    while (1) {
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '+') {
            s++; int r = parse_number(&s); value += r; continue;
        } else if (*s == '-') {
            s++; int r = parse_number(&s); value -= r; continue;
        }
        break;
    }
    *p = s;
    return value;
}

int mathparse(const char *expr, int base) {
    if (!expr || expr[0]=='\0') return 0;
    char tmp[512]; strncpy(tmp, expr, 511); tmp[511]='\0'; trim(tmp);
    // uppercase tmp for symbol lookup
    for (int k = 0; tmp[k]; k++) tmp[k] = toupper((unsigned char)tmp[k]);
    // check symbols exact match
    for (int i=0;i<symcnt;i++) if (strcmp(sym[i], tmp)==0) return atoi(symval[i]);
    // use expression parser
    const char *p = tmp;
    int v = parse_expr(&p);
    return v;
}

// doasm implementation
void doasm(const char *line) {
    char s[1024];
    int adr, reladr;

    // Copy the line and process
    strncpy(s, line, 1023);
    s[1023] = '\0';
    trim(s);

    // Remove inline comments: everything after ';' is a comment
    char *inline_comment = strchr(s, ';');
    if (inline_comment) {
        *inline_comment = '\0';
        trim(s);
    }

    // Ignore empty lines and comments
    if (s[0] == '\0') return;

    // Ignore lines that contain only whitespace
    bool only_whitespace = true;
    for (int ii = 0; s[ii] != '\0'; ii++) {
        if (s[ii] != ' ' && s[ii] != '\t') { only_whitespace = false; break; }
    }
    if (only_whitespace) return;

    // Handle labels (lines containing ':') BEFORE extractop
    char *colon = strchr(s, ':');
    if (colon != NULL) {
        char *comment = strchr(s, ';');
        if (comment == NULL || colon < comment) {
            *colon = '\0';
            trim(s);
            char *after_colon = colon + 1;
            trim(after_colon);
            // Se la riga dopo la label è una direttiva .DW, registra la label PRIMA di processare la direttiva
            if (strlen(s) > 0) {
                addlabel(s); // registra la label
            }
            if (strlen(after_colon) > 0 && after_colon[0] != ';') {
                doasm(after_colon); // processa la direttiva o istruzione
            }
            return;
        }
    }

    // Handle assembler directives
    if (s[0] == '.') {
        // .org directive
        if (strncmp(s, ".org", 4) == 0 || strncmp(s, ".ORG", 4) == 0) {
            char *addr_str = s + 4;
            while (*addr_str == ' ' || *addr_str == '\t') addr_str++;
            startadr = mathparse(addr_str, 10);
            printf("ORG: Start address set to %d\n", startadr);
            return;
        }
        // .equ directive
        if (strncmp(s, ".equ", 4) == 0 || strncmp(s, ".EQU", 4) == 0) {
            char *rest = s + 4;
            while (*rest == ' ' || *rest == '\t') rest++;
            char *space = strchr(rest, ' ');
            if (!space) space = strchr(rest, '\t');
            if (space) {
                *space = '\0';
                char *value = space + 1;
                while (*value == ' ' || *value == '\t') value++;
                char val_str[32];
                snprintf(val_str, 31, "%d", mathparse(value, 10));
                addsymbol(rest, val_str);
                printf("EQU: %s = %s\n", rest, val_str);
            }
            return;
        }
        // .include directive (supports quotes or not)
        if (strncasecmp(s, ".include", 8) == 0) {
            char *rest = s + 8;
            while (*rest == ' ' || *rest == '\t') rest++;
            char fname[256];
            if (*rest == '"' || *rest == '\'') {
                char quote = *rest;
                rest++;
                int i = 0;
                while (*rest && *rest != quote && i < 255) fname[i++] = *rest++;
                fname[i] = '\0';
            } else {
                int i = 0;
                while (*rest && *rest != ' ' && *rest != '\t' && *rest != ';' && i < 255) fname[i++] = *rest++;
                fname[i] = '\0';
            }
            FILE *inc = fopen(fname, "r");
            if (!inc) {
                fprintf(stderr, "Line %d: Include file '%s' not found! in file %s\n", cline + 1, fname, cf);
                exit(1);
            }
            char line[1024];
            while (fgets(line, sizeof(line), inc)) {
                trim(line);
                if (line[0] != '\0') doasm(line);
            }
            fclose(inc);
            return;
        }
        // .db directive - define data bytes
        if (strncasecmp(s, ".db", 3) == 0) {
            char *rest = s + 3;
            while (*rest == ' ' || *rest == '\t') rest++;
            char *token = strtok(rest, ",");
            while (token != NULL) {
                while (*token == ' ' || *token == '\t') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && (*end == ' ' || *end == '\t')) end--;
                *(end+1) = '\0';
                // Strip inline comment from token if present
                char *sc = strchr(token, ';');
                if (sc) *sc = '\0';
                trim(token);
                if (strlen(token) > 0) {
                    int val = mathparse(token, 0);
                    if (debug_enabled) {
                        write_debug(".DB token='%s' val=%d\n", token, val);
                    }
                    addcode(val);
                }
                token = strtok(NULL, ",");
            }
            // dump after .DB for debugging
            if (debug_enabled && cline == 5) {
                write_debug("DEBUG-SNAP: after .DB codpos=%d:\n", codpos);
                for (int k = 0; k < codpos; k++) write_debug("%02X ", code[k]);
                write_debug("\n");
            }
            return;
        }
        // .dw directive - define data words (big endian)
        if (strncasecmp(s, ".dw", 3) == 0) {
            char *rest = s + 3;
            while (*rest == ' ' || *rest == '\t') rest++;
            char *token = strtok(rest, ",");
            while (token != NULL) {
                while (*token == ' ' || *token == '\t') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && (*end == ' ' || *end == '\t')) end--;
                *(end+1) = '\0';
                // Strip inline comment from token if present
                char *sc = strchr(token, ';');
                if (sc) *sc = '\0';
                trim(token);
                if (strlen(token) > 0) {
                    int val = mathparse(token, 0);
                    // Big endian: MSB first
                    addcode((val >> 8) & 0xFF);
                    addcode(val & 0xFF);
                }
                token = strtok(NULL, ",");
            }
            return;
        }
        return; // Ignore other directives not implemented
    }

    // Extract operation and parameters ONLY if not a label or directive
    extractop(s);
    if (debug_enabled) {
        write_debug("op='%s' param1='%s' param2='%s'\n", op, param1, param2);
    }

    // Clean param1 removing trailing commas
    if (strlen(param1) > 0 && param1[strlen(param1) - 1] == ',') {
        param1[strlen(param1) - 1] = '\0';
    }
    trim(param1);
    trim(param2);

    // Search opcode in table
    if (findop(op)) {
        // Absolute jumps
        if (opp == 120) { // CALL
            adr = calcadr();
            if (adr > 0) {
                if (adr < 8192) {
                    addcode(0xE0 + (adr / 256));
                    addcode(adr % 256);
                } else {
                    addcode(opp);
                    addcode(adr / 256);
                    addcode(adr % 256);
                }
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        } else if (opp == 121 || opp == 16 || (opp >= 124 && opp <= 127)) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        // Relative jumps
        else if (in_set(opp, JR, 11)) {
            adr = calcadr();
            write_debug("JR/JRPLUS codpos=%d startadr=%d adr=%d param1=%s\n", codpos, startadr, adr, param1);
            addcode(opp);
            if (adr >= 8192) {
                if (in_set(opp, JRPLUS, 5)) {
                    write_debug("JRPLUS offset used = %d\n", adr - codpos - startadr);
                    addcode(adr - codpos - startadr);
                } else {
                    write_debug("JRMINUS offset used = %d\n", (codpos + startadr) - adr);
                    addcode((codpos + startadr) - adr);
                }
            } else if (adr > 0) {
                write_debug("JR direct adr used = %d\n", adr);
                addcode(adr);
            } else {
                write_debug("JR placeholder 0 (label unresolved)\n");
                addcode(0);
            }
            if (debug_enabled && cline == 9) {
                write_debug("after JRM codpos=%d:\n", codpos);
                for (int k = 0; k < codpos; k++) write_debug("%02X ", code[k]);
                write_debug("\n");
            }
        }
        // Normal instructions
        else {
            addcode(opp);
            if (NBARGU[opp] == 2) {
                if (strlen(param1) > 0) {
                    int val = mathparse(param1, 0);
                    // Accetta anche "0x00", "0b0", "00" come zero valido
                    bool is_zero_valid = (val == 0) && (
                        strcmp(param1, "0") == 0 ||
                        strcasecmp(param1, "0x0") == 0 ||
                        strcasecmp(param1, "0x00") == 0 ||
                        strcasecmp(param1, "0b0") == 0 ||
                        strcasecmp(param1, "00") == 0
                    );
                    // Accetta anche LB(...) e HB(...) con simboli non risolti
                    bool is_lb_hb_unresolved = false;
                    if (val == 0 && (strncmp(param1, "LB(", 3) == 0 || strncmp(param1, "HB(", 3) == 0)) {
                        // Estrai il nome del simbolo tra parentesi
                        const char *start = param1 + 3;
                        const char *end = strchr(start, ')');
                        if (end && end > start) {
                            char symbol[256];
                            size_t len = end - start;
                            if (len < sizeof(symbol)) {
                                strncpy(symbol, start, len);
                                symbol[len] = '\0';
                                // Rimuovi eventuali spazi
                                trim(symbol);
                                // Se il simbolo non è risolto, accetta senza warning
                                if (!findsymbol(symbol)) {
                                    is_lb_hb_unresolved = true;
                                }
                            }
                        }
                    }
                    if (val == 0 && !is_zero_valid && !is_lb_hb_unresolved) {
                        printf("Warning: invalid parameter for %s: '%s' at line %d\n", op, param1, cline);
                        return;
                    }
                    addcode(val);
                } else {
                    printf("Warning: missing parameter for %s at line %d\n", op, cline);
                    return;
                }
            } else if (NBARGU[opp] == 3) {
                if (param2[0] == '\0') {
                    if (strlen(param1) == 0) {
                        printf("Warning: missing parameter for %s at line %d\n", op, cline);
                        return;
                    }
                    int val = mathparse(param1, 16);
                    if (val == 0 && strcmp(param1, "0") != 0) {
                        printf("Warning: invalid parameter for %s: '%s' at line %d\n", op, param1, cline);
                        return;
                    }
                    addcode((val >> 8) & 0xFF);
                    addcode(val & 0xFF);
                } else {
                    if (strlen(param1) > 0) {
                        int val1 = mathparse(param1, 0);
                        if (val1 == 0 && strcmp(param1, "0") != 0) {
                            printf("Warning: invalid parameter for %s: '%s' at line %d\n", op, param1, cline);
                            return;
                        }
                        addcode(val1);
                    } else {
                        printf("Warning: missing parameter for %s at line %d\n", op, cline);
                        return;
                    }
                    if (strlen(param2) > 0) {
                        int val2 = mathparse(param2, 0);
                        if (val2 == 0 && strcmp(param2, "0") != 0) {
                            printf("Warning: invalid parameter for %s: '%s' at line %d\n", op, param2, cline);
                            return;
                        }
                        addcode(val2);
                    } else {
                        printf("Warning: missing parameter for %s at line %d\n", op, cline);
                        return;
                    }
                }
            }
        }
    }
    // Special instructions not in OPCODE table
    else {
        if (strcmp(op, "ADD") == 0) {
            if (strcmp(param1, "[P]") == 0 && strcmp(param2, "A") == 0) {
                addcode(0x44);
            } else if (strcmp(param1, "[P]") == 0) {
                addcode(0x70);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "A") == 0) {
                addcode(0x74);
                addcode(mathparse(param2, 8));
            }
        }
        else if (strcmp(op, "MOV") == 0) {
            if (strcmp(param1, "A") == 0 && strcmp(param2, "[+X]") == 0) {
                addcode(36);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[-X]") == 0) {
                addcode(37);
            } else if (strcmp(param1, "[+Y]") == 0 && strcmp(param2, "A") == 0) {
                addcode(38);
            } else if (strcmp(param1, "[-Y]") == 0 && strcmp(param2, "A") == 0) {
                addcode(39);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "R") == 0) {
                addcode(34);
            } else if (strcmp(param1, "R") == 0 && strcmp(param2, "A") == 0) {
                addcode(50);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "Q") == 0) {
                addcode(33);
            } else if (strcmp(param1, "Q") == 0 && strcmp(param2, "A") == 0) {
                addcode(49);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "P") == 0) {
                addcode(32);
            } else if (strcmp(param1, "P") == 0 && strcmp(param2, "A") == 0) {
                addcode(48);
            } else if (strcmp(param1, "[P]") == 0 && strcmp(param2, "[DP]") == 0) {
                addcode(85);
            } else if (strcmp(param1, "[DP]") == 0 && strcmp(param2, "[P]") == 0) {
                addcode(83);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[DP]") == 0) {
                addcode(87);
            } else if (strcmp(param1, "[DP]") == 0 && strcmp(param2, "A") == 0) {
                addcode(82);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[P]") == 0) {
                addcode(89);
            } else if (strcmp(param1, "[P]") == 0 && strcmp(param2, "A") == 0) {
                addcode(219);
                addcode(89);
            } else if (strcmp(param1, "A") == 0) {
                addcode(2);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "B") == 0) {
                addcode(3);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "I") == 0) {
                addcode(0);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "J") == 0) {
                addcode(1);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "P") == 0) {
                addcode(18);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "Q") == 0) {
                addcode(19);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "DPL") == 0) {
                addcode(17);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "DP") == 0) {
                addcode(16);
                int val = mathparse(param2, 16);
                addcode(val / 256);
                addcode(val % 256);
            }
        }
        else if (strcmp(op, "NOP") == 0) {
            if (param1[0] == '\0') {
                addcode(77); // NOP constant
            } else {
                addcode(78);
                addcode(mathparse(param1, 8));
            }
        }
        else if (strcmp(op, "LP") == 0) {
            int val = mathparse(param1, 8);
            if (val > 63) abort_c("LP command exceeds range!");
            addcode(128 + val);
        }
        else if (strcmp(op, "RTN") == 0) {
            addcode(55);
        }
        else if (strcmp(op, "CALL") == 0) {
            adr = calcadr();
            if (adr > 0) {
                if (adr < 8192) {
                    addcode(0xE0 + adr / 256);
                    addcode(adr % 256);
                } else {
                    addcode(0x78);
                    addcode(adr / 256);
                    addcode(adr % 256);
                }
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "RJMP") == 0) {
            adr = calcadr();
            if (adr >= 8192) {
                if (abs(adr - startadr - codpos) < 255) {
                    if (adr > startadr + codpos) addcode(44);
                    else addcode(45);
                    addcode(abs(adr - startadr - codpos));
                } else {
                    addcode(121); addcode(adr / 256); addcode(adr % 256); // Do absolute jump then
                }
            } else if ((adr >= 1) && (adr <= 255)) {
                addcode(44); addcode(adr);
            } else if ((adr <= -1) && (adr >= -255)) {
                addcode(45); addcode(adr);
            } else {
                addcode(NOP); addcode(NOP);
            }
        }
        else if (strcmp(op, "BRLO") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRHI") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BREQ") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRNE") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRLT") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRGT") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRSH") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRCS") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRIE") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "BRAN") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else {
            printf("Warning: Unknown instruction '%s' at line %d\n", op, cline);
        }
    }
}


// resolve_labels_and_write_bin implementation
void resolve_labels_and_write_bin(const char *output_filename) {
    FILE *fout;
    int i, j;
    bool resolved;

    if (debug_enabled) {
        write_debug("nlabcnt=%d\n", nlabcnt);
        for (i = 0; i < nlabcnt; i++) write_debug("nlab[%d]=%s nlabpos=%d nlabasm=%s\n", i, nlab[i], nlabpos[i], nlabasm[i]);
    }

    // Resolve undefined labels with multiple passes
    for (int pass = 0; pass < 10 && nlabcnt > 0; pass++) {
        resolved = false;

        for (i = 0; i < nlabcnt; i++) {
            if (findlabel(nlab[i])) {
                int saved_codpos = codpos;
                int saved_opp = opp;
                char saved_op[256], saved_params[256], saved_param1[256], saved_param2[256];

                // Save current state
                strcpy(saved_op, op);
                strcpy(saved_params, params);
                strcpy(saved_param1, param1);
                strcpy(saved_param2, param2);

                // Reposition code to the unresolved label position
                codpos = nlabpos[i];

                // Reassemble the instruction now that the label is resolved
                extractop(nlabasm[i]);
                if (findop(op)) {
                    // Rivaluta parametri con mathparse ora che la label è risolta
                    int val1 = 0, val2 = 0;
                    if (strlen(param1) > 0) val1 = mathparse(param1, 0);
                    if (strlen(param2) > 0) val2 = mathparse(param2, 0);
                    // Aggiorna il codice in base al tipo di istruzione
                    if (NBARGU[opp] == 2) {
                        code[nlabpos[i]] = opp;
                        code[nlabpos[i]+1] = val1;
                    } else if (NBARGU[opp] == 3) {
                        code[nlabpos[i]] = opp;
                        code[nlabpos[i]+1] = (val1 >> 8) & 0xFF;
                        code[nlabpos[i]+2] = val1 & 0xFF;
                    } else if (NBARGU[opp] == 4) {
                        code[nlabpos[i]] = opp;
                        code[nlabpos[i]+1] = val1;
                        code[nlabpos[i]+2] = val2;
                    }
                    // Gestione jump/call come già presente
                    if (in_set(opp, JR, 11)) {
                        int dest = labpos[labp];
                        int offset = dest - (nlabpos[i] + 1);
                        code[nlabpos[i]] = opp;
                        code[nlabpos[i] + 1] = offset & 0xFF;
                    } else if (opp == 120 || opp == 121 || opp == 16 || (opp >= 124 && opp <= 127)) {
                        int dest = labpos[labp] + labbase[labp];
                        code[nlabpos[i]] = opp;
                        code[nlabpos[i] + 1] = (dest / 256) & 0xFF;
                        code[nlabpos[i] + 2] = dest & 0xFF;
                    }
                }

                // Restore state
                codpos = saved_codpos;
                opp = saved_opp;
                strcpy(op, saved_op);
                strcpy(params, saved_params);
                strcpy(param1, saved_param1);
                strcpy(param2, saved_param2);

                write_debug("Resolved label: %s -> %d\n", nlab[i], labpos[labp] + startadr);

                // Remove the unresolved label from the list
                delnlabel(i);
                i--; // Decrement to compensate for removal
                resolved = true;
            }
        }

        if (!resolved) break; // No labels resolved in this pass
    }

    if (nlabcnt > 0) {
        printf("Warning: %d unresolved labels:\n", nlabcnt);
        for (i = 0; i < nlabcnt; i++) {
            printf("  %s (line with instruction: %s)\n", nlab[i], nlabasm[i]);
        }
    }

    // Write the binary file
    fout = fopen(output_filename, "wb");
    if (!fout) {
        perror("Error creating output file");
        return;
    }

    size_t written = fwrite(code, 1, codpos, fout);
    fclose(fout);

    printf("Binary file written: %s (%zu bytes)\n", output_filename, written);
    printf("Start address: %d (0x%X)\n", startadr, startadr);
    printf("Code size: %d bytes\n", codpos);

    // Print defined symbols and labels
    if (symcnt > 0) {
        printf("\nDefined symbols:\n");
        for (i = 0; i < symcnt; i++) {
            printf("  %s = %s\n", sym[i], symval[i]);
        }
    }

    if (labcnt > 0) {
        printf("\nDefined labels:\n");
        for (i = 0; i < labcnt; i++) {
            printf("  %s: %d (0x%X)\n", lab[i], labpos[i] + startadr, labpos[i] + startadr);
        }
    }
}

// Show help
void show_help(const char *program_name) {
    printf("Usage: %s [options] <file.asm> <file.bin>\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --debug    Enable generation of debug.txt\n");
    printf("  -h, --help     Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s program.asm program.bin\n", program_name);
    printf("  %s -d program.asm program.bin    (with debug enabled)\n", program_name);
}

// main
int main(int argc, char *argv[]) {
    char line[1024];
    FILE *fin;
    char *input_file = NULL;
    char *output_file = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_enabled = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            show_help(argv[0]);
            return 1;
        } else {
            if (input_file == NULL) {
                input_file = argv[i];
            } else if (output_file == NULL) {
                output_file = argv[i];
            } else {
                fprintf(stderr, "Too many arguments!\n");
                show_help(argv[0]);
                return 1;
            }
        }
    }

    if (input_file == NULL || output_file == NULL) {
        fprintf(stderr, "Error: specify input and output files\n");
        show_help(argv[0]);
        return 1;
    }

    // Open debug file only if requested
    open_debug_file();

    if (debug_enabled) {
        printf("Debug enabled: generating debug.txt\n");
    }

    fin = fopen(input_file, "r");
    if (!fin) {
        perror("Error opening source file");
        close_debug_file();
        return 1;
    }

    strcpy(cf, input_file); // Save filename for error messages
    cline = 0;

    while (fgets(line, sizeof(line), fin)) {
        cline++;
        line[strcspn(line, "\r\n")] = 0;
        doasm(line);
    }
    fclose(fin);

    printf("Assembly completed. Bytes generated: %d\n", codpos);

    resolve_labels_and_write_bin(output_file);
    close_debug_file();

    return 0;
}
