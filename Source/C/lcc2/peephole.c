#include "peephole.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_range(const char *text, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

static char *replace_matches(const char *text, const char *pattern,
                             char **replacement, const char *suffix)
{
    char *result = duplicate_range(text, strlen(text));
    size_t suffix_length = strlen(suffix);

    if (result == NULL) {
        return NULL;
    }
    for (;;) {
        char *match = strstr(result, pattern);
        size_t prefix, replacement_length, tail_length;
        char *grown, *next_replacement;

        if (match == NULL) {
            return result;
        }
        prefix = (size_t)(match - result);
        replacement_length = strlen(*replacement);
        tail_length = strlen(match + strlen(pattern));
        if (prefix > (size_t)-1 - replacement_length ||
            prefix + replacement_length > (size_t)-1 - tail_length - 1) {
            free(result);
            return NULL;
        }
        grown = malloc(prefix + replacement_length + tail_length + 1);
        if (grown == NULL) {
            free(result);
            return NULL;
        }
        memcpy(grown, result, prefix);
        memcpy(grown + prefix, *replacement, replacement_length);
        memcpy(grown + prefix + replacement_length,
               match + strlen(pattern), tail_length + 1);
        free(result);
        result = grown;

        if (replacement_length > (size_t)-1 - suffix_length - 1) {
            free(result);
            return NULL;
        }
        next_replacement = malloc(replacement_length + suffix_length + 1);
        if (next_replacement == NULL) {
            free(result);
            return NULL;
        }
        memcpy(next_replacement, *replacement, replacement_length);
        memcpy(next_replacement + replacement_length, suffix,
               suffix_length + 1);
        free(*replacement);
        *replacement = next_replacement;
    }
}

static char *optimize_initial_incdec(const char *assembly)
{
    char pattern[192];
    char *result = duplicate_range(assembly, strlen(assembly));
    char *increment_replacement = duplicate_range("\tINCA", 5);
    char *decrement_replacement = duplicate_range("\tDECA", 5);
    size_t value;
    const char *newlines[] = {"\r\n", "\n"};
    size_t newline_index;

    if (result == NULL || increment_replacement == NULL ||
        decrement_replacement == NULL) {
        free(result);
        free(increment_replacement);
        free(decrement_replacement);
        return NULL;
    }
    for (value = 1; value <= 6; ++value) {
        for (newline_index = 0; newline_index < 2; ++newline_index) {
            const char *newline = newlines[newline_index];
            char suffix[32];
            char *next;

            (void)snprintf(pattern, sizeof(pattern),
                "\tLIA\t%lu\t; Load constant %lu%s\tEXAB%s\tLP\t3%s"
                "\tSBM\t\t; Subtraction%s\tEXAB",
                (unsigned long)value, (unsigned long)value, newline,
                newline, newline, newline);
            (void)snprintf(suffix, sizeof(suffix), "%s\tDECA", newline);
            next = replace_matches(result, pattern, &increment_replacement,
                                   suffix);
            free(result);
            result = next;
            if (result == NULL) {
                goto failure;
            }

            (void)snprintf(pattern, sizeof(pattern),
                "\tLIA\t%lu\t; Load constant %lu%s\tLP\t3%s"
                "\tADM\t\t; Addition%s\tEXAB",
                (unsigned long)value, (unsigned long)value, newline,
                newline, newline);
            (void)snprintf(suffix, sizeof(suffix), "%s\tINCA", newline);
            next = replace_matches(result, pattern, &decrement_replacement,
                                   suffix);
            free(result);
            result = next;
            if (result == NULL) {
                goto failure;
            }
        }
    }
    free(increment_replacement);
    free(decrement_replacement);
    return result;

failure:
    free(increment_replacement);
    free(decrement_replacement);
    return NULL;
}

typedef struct AssemblyLine {
    char *text;
    char opcode[16];
    char operand[32];
} AssemblyLine;

typedef struct LineList {
    AssemblyLine *items;
    size_t count;
    size_t capacity;
} LineList;

static void parse_line(AssemblyLine *line)
{
    const char *cursor = line->text;
    size_t length;

    line->opcode[0] = '\0';
    line->operand[0] = '\0';
    while (isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    length = 0;
    while (cursor[length] != '\0' &&
           !isspace((unsigned char)cursor[length]) &&
           cursor[length] != ';') {
        ++length;
    }
    if (length >= sizeof(line->opcode)) {
        return;
    }
    memcpy(line->opcode, cursor, length);
    line->opcode[length] = '\0';
    cursor += length;
    while (isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    length = 0;
    while (cursor[length] != '\0' &&
           !isspace((unsigned char)cursor[length]) &&
           cursor[length] != ';') {
        ++length;
    }
    if (length < sizeof(line->operand)) {
        memcpy(line->operand, cursor, length);
        line->operand[length] = '\0';
    }
}

static int append_line(LineList *lines, const char *text, size_t length)
{
    AssemblyLine *grown;
    char *copy;

    if (lines->count == lines->capacity) {
        size_t capacity = lines->capacity == 0 ? 32 : lines->capacity * 2;
        if (capacity < lines->capacity ||
            capacity > (size_t)-1 / sizeof(*lines->items)) {
            return -1;
        }
        grown = realloc(lines->items, capacity * sizeof(*lines->items));
        if (grown == NULL) {
            return -1;
        }
        lines->items = grown;
        lines->capacity = capacity;
    }
    copy = malloc(length + 1);
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    lines->items[lines->count].text = copy;
    parse_line(&lines->items[lines->count]);
    ++lines->count;
    return 0;
}

static void delete_line(LineList *lines, size_t index)
{
    free(lines->items[index].text);
    memmove(&lines->items[index], &lines->items[index + 1],
            (lines->count - index - 1) * sizeof(*lines->items));
    --lines->count;
}

static int replace_opcode(AssemblyLine *line, const char *opcode)
{
    const char *start = line->text;
    const char *end;
    size_t prefix, suffix, opcode_length = strlen(opcode);
    char *replacement;

    while (isspace((unsigned char)*start)) {
        ++start;
    }
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end) && *end != ';') {
        ++end;
    }
    prefix = (size_t)(start - line->text);
    suffix = strlen(end);
    replacement = malloc(prefix + opcode_length + suffix + 1);
    if (replacement == NULL) {
        return -1;
    }
    memcpy(replacement, line->text, prefix);
    memcpy(replacement + prefix, opcode, opcode_length);
    memcpy(replacement + prefix + opcode_length, end, suffix + 1);
    free(line->text);
    line->text = replacement;
    parse_line(line);
    return 0;
}

static int replace_instruction(AssemblyLine *line, const char *opcode)
{
    size_t length = strlen(opcode);
    size_t newline_length = 0;
    char *replacement;

    if (strchr(line->text, '\n') != NULL) {
        newline_length = 1;
    }
    if (newline_length != 0 && line->text[strlen(line->text) - 2] == '\r') {
        newline_length = 2;
    }
    replacement = malloc(length + newline_length + 2);
    if (replacement == NULL) {
        return -1;
    }
    replacement[0] = '\t';
    memcpy(replacement + 1, opcode, length);
    if (newline_length != 0) {
        memcpy(replacement + length + 1,
               line->text + strlen(line->text) - newline_length,
               newline_length);
    }
    replacement[length + newline_length + 1] = '\0';
    free(line->text);
    line->text = replacement;
    parse_line(line);
    return 0;
}

static void remove_double_exab(LineList *lines)
{
    size_t i = 0;
    while (i + 1 < lines->count) {
        if (strcmp(lines->items[i].opcode, "EXAB") == 0 &&
            strcmp(lines->items[i + 1].opcode, "EXAB") == 0) {
            delete_line(lines, i + 1);
            delete_line(lines, i);
        } else {
            ++i;
        }
    }
}

static int replace_lia_exab(LineList *lines)
{
    size_t i = 0;
    while (i + 1 < lines->count) {
        if (strcmp(lines->items[i].opcode, "LIA") == 0 &&
            strcmp(lines->items[i + 1].opcode, "EXAB") == 0) {
            if (replace_opcode(&lines->items[i], "LIB") != 0) {
                return -1;
            }
            delete_line(lines, i + 1);
        }
        ++i;
    }
    return 0;
}

static void remove_redundant_pushpop(LineList *lines)
{
    size_t i = 0;
    while (i + 2 < lines->count) {
        if (strcmp(lines->items[i].opcode, "PUSH") == 0 &&
            strcmp(lines->items[i + 1].opcode, "LIB") == 0 &&
            strcmp(lines->items[i + 2].opcode, "POP") == 0) {
            delete_line(lines, i + 2);
            delete_line(lines, i);
        } else {
            ++i;
        }
    }
}

static int is_lib_one(const AssemblyLine *line)
{
    return strcmp(line->opcode, "LIB") == 0 &&
           strcmp(line->operand, "1") == 0;
}

static int optimize_incdec(LineList *lines)
{
    size_t i = 0;
    while (i + 3 < lines->count) {
        if (is_lib_one(&lines->items[i]) &&
            strcmp(lines->items[i + 1].opcode, "LP") == 0 &&
            strcmp(lines->items[i + 1].operand, "3") == 0 &&
            strcmp(lines->items[i + 2].opcode, "ADM") == 0 &&
            strcmp(lines->items[i + 3].opcode, "EXAB") == 0) {
            if (replace_instruction(&lines->items[i], "INCA") != 0) {
                return -1;
            }
            delete_line(lines, i + 3);
            delete_line(lines, i + 2);
            delete_line(lines, i + 1);
        } else if (i + 4 < lines->count &&
                   is_lib_one(&lines->items[i]) &&
                   strcmp(lines->items[i + 1].opcode, "LP") == 0 &&
                   strcmp(lines->items[i + 1].operand, "3") == 0 &&
                   strcmp(lines->items[i + 2].opcode, "EXAB") == 0 &&
                   strcmp(lines->items[i + 3].opcode, "SBM") == 0 &&
                   strcmp(lines->items[i + 4].opcode, "EXAB") == 0) {
            if (replace_instruction(&lines->items[i], "DECA") != 0) {
                return -1;
            }
            delete_line(lines, i + 4);
            delete_line(lines, i + 3);
            delete_line(lines, i + 2);
            delete_line(lines, i + 1);
        } else {
            ++i;
        }
    }
    return 0;
}

char *peephole_optimize(const char *assembly)
{
    LineList lines = {0};
    char *initial;
    const char *cursor;
    size_t output_length = 0, index;
    char *output, *destination;

    if (assembly == NULL) {
        return NULL;
    }
    initial = optimize_initial_incdec(assembly);
    if (initial == NULL) {
        return NULL;
    }
    cursor = initial;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t length = end == NULL ? strlen(cursor) :
                                      (size_t)(end - cursor) + 1;
        if (append_line(&lines, cursor, length) != 0) {
            free(initial);
            goto failure;
        }
        cursor += length;
    }
    free(initial);

    /* Pass order intentionally matches Backend.pas. */
    if (replace_lia_exab(&lines) != 0) {
        goto failure;
    }
    remove_redundant_pushpop(&lines);
    remove_double_exab(&lines);
    if (optimize_incdec(&lines) != 0) {
        goto failure;
    }

    for (index = 0; index < lines.count; ++index) {
        size_t length = strlen(lines.items[index].text);
        if (length > (size_t)-1 - output_length - 1) {
            goto failure;
        }
        output_length += length;
    }
    output = malloc(output_length + 1);
    if (output == NULL) {
        goto failure;
    }
    destination = output;
    for (index = 0; index < lines.count; ++index) {
        size_t length = strlen(lines.items[index].text);
        memcpy(destination, lines.items[index].text, length);
        destination += length;
    }
    *destination = '\0';
    for (index = 0; index < lines.count; ++index) {
        free(lines.items[index].text);
    }
    free(lines.items);
    return output;

failure:
    for (index = 0; index < lines.count; ++index) {
        free(lines.items[index].text);
    }
    free(lines.items);
    return NULL;
}
