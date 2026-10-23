#include "codegen.h"
#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    fprintf(stream, "Usage: lcc2 source.c -o output.asm\n"
                    "       lcc2 --help | --version\n");
}

static char *read_source(const char *path)
{
    FILE *file = fopen(path, "rb");
    char *source = NULL;
    size_t length = 0;
    char chunk[4096];
    size_t count;

    if (file == NULL) {
        fprintf(stderr, "lcc2: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    while ((count = fread(chunk, 1, sizeof(chunk), file)) != 0) {
        char *grown;
        if (memchr(chunk, '\0', count) != NULL) {
            fprintf(stderr, "lcc2: source contains a NUL byte\n");
            free(source);
            fclose(file);
            return NULL;
        }
        if (length > (size_t)-1 - count - 1 ||
            (grown = realloc(source, length + count + 1)) == NULL) {
            fprintf(stderr, "lcc2: source is too large or out of memory\n");
            free(source);
            fclose(file);
            return NULL;
        }
        source = grown;
        memcpy(source + length, chunk, count);
        length += count;
        source[length] = '\0';
    }
    {
        int failed = ferror(file);
        if (fclose(file) != 0) {
            failed = 1;
        }
        if (failed) {
            fprintf(stderr, "lcc2: cannot read '%s'\n", path);
            free(source);
            return NULL;
        }
    }
    if (source == NULL) {
        source = calloc(1, 1);
        if (source == NULL) {
            fprintf(stderr, "lcc2: out of memory\n");
        }
    }
    return source;
}

int main(int argc, char **argv)
{
    const char *input = NULL, *output = NULL;
    char *source;
    AstArena *arena;
    AstTranslationUnit *unit = NULL;
    CodegenContext *ctx;
    FILE *file;
    int index, status = 1;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("lcc2 SC61860 C compiler prototype");
        return 0;
    }

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "-o") == 0 && output == NULL && index + 1 < argc) {
            output = argv[++index];
        } else if (argv[index][0] != '-' && input == NULL) {
            input = argv[index];
        } else {
            print_usage(stderr);
            return 2;
        }
    }
    if (input == NULL || output == NULL || strcmp(input, output) == 0) {
        print_usage(stderr);
        return 2;
    }
    source = read_source(input);
    if (source == NULL) {
        return 1;
    }
    arena = ast_arena_create();
    ctx = codegen_context_create();
    if (arena == NULL || ctx == NULL) {
        fprintf(stderr, "lcc2: out of memory\n");
        goto cleanup;
    }
    if (lcc2_parse_string(arena, source, &unit) != 0) {
        fprintf(stderr, "lcc2: cannot parse '%s'\n", input);
        goto cleanup;
    }
    if (codegen_program(ctx, unit) != 0) {
        fprintf(stderr, "lcc2: %s\n", codegen_error(ctx));
        goto cleanup;
    }
    file = fopen(output, "wb");
    if (file == NULL) {
        fprintf(stderr, "lcc2: cannot open '%s': %s\n", output, strerror(errno));
        goto cleanup;
    }
    {
        int failed = fputs(codegen_output(ctx), file) == EOF;
        if (fclose(file) != 0) {
            failed = 1;
        }
        if (failed) {
            fprintf(stderr, "lcc2: cannot write '%s'\n", output);
            if (remove(output) != 0) {
                fprintf(stderr, "lcc2: cannot remove incomplete output '%s'\n", output);
            }
            goto cleanup;
        }
    }
    status = 0;

cleanup:
    codegen_context_destroy(ctx);
    ast_arena_destroy(arena);
    free(source);
    return status;
}
