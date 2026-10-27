#include "codegen.h"
#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    fprintf(stream, "Usage: lcc2 [--target PC1403] source.c -o output.asm\n"
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

static char *read_runtime(const char *directory, const char *routine)
{
    char path[512];
    FILE *file;
    char *runtime = NULL;
    size_t length = 0;
    char chunk[4096];
    size_t count;
    const char *formats[] = {
        "%s/%s.asm",
        "Source/C/lcc2/%s/%s.asm"
    };
    size_t format_index;

    for (format_index = 0;
         format_index < sizeof(formats) / sizeof(formats[0]);
         ++format_index) {
        (void)snprintf(path, sizeof(path), formats[format_index], directory, routine);
        file = fopen(path, "rb");
        if (file != NULL) {
            break;
        }
    }
    if (file == NULL) {
        fprintf(stderr, "lcc2: cannot find runtime '%s' in '%s'\n",
                routine, directory);
        return NULL;
    }
    while ((count = fread(chunk, 1, sizeof(chunk), file)) != 0) {
        char *grown;
        if (memchr(chunk, '\0', count) != NULL ||
            length > (size_t)-1 - count - 1 ||
            (grown = realloc(runtime, length + count + 1)) == NULL) {
            fprintf(stderr, "lcc2: invalid or oversized runtime '%s'\n",
                    path);
            free(runtime);
            fclose(file);
            return NULL;
        }
        runtime = grown;
        memcpy(runtime + length, chunk, count);
        length += count;
        runtime[length] = '\0';
    }
    {
        int failed = ferror(file);
        if (fclose(file) != 0) {
            failed = 1;
        }
        if (failed) {
            fprintf(stderr, "lcc2: cannot read runtime '%s'\n", path);
            free(runtime);
            return NULL;
        }
    }
    if (runtime == NULL) {
        runtime = calloc(1, 1);
        if (runtime == NULL) {
            fprintf(stderr, "lcc2: out of memory reading runtime '%s'\n",
                    path);
        }
    }
    return runtime;
}

static int function_defined(const AstTranslationUnit *unit, const char *name)
{
    size_t index;
    for (index = 0; index < unit->declarations.count; ++index) {
        const AstDecl *declaration = unit->declarations.items[index];
        if (declaration->node.kind == AST_NODE_FUNCTION_DECL) {
            const AstFunctionDecl *function = (const AstFunctionDecl *)declaration;
            if (!function->is_prototype && strcmp(function->name, name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *input = NULL, *output = NULL;
    const char *target = NULL;
    static const struct {
        const char *name;
        const char *directory;
        const char *dependency;
    } runtime_modules[] = {
        {"strcpy", "runtime", NULL},
        {"strcat", "runtime", NULL},
        {"int2str", "runtime", NULL},
        {"puts", "runtime", "putchar"},
        {"getchar", NULL, NULL},
        {"putchar", NULL, NULL}
    };
    char *runtimes[sizeof(runtime_modules) / sizeof(runtime_modules[0])] = {NULL};
    int needed[sizeof(runtime_modules) / sizeof(runtime_modules[0])] = {0};
    char target_directory[64];
    char *source;
    size_t runtime_index;
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
        if (strcmp(argv[index], "--target") == 0 && target == NULL &&
            index + 1 < argc) {
            target = argv[++index];
        } else if (strcmp(argv[index], "-o") == 0 && output == NULL &&
                   index + 1 < argc) {
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
    if (target != NULL && strcmp(target, "PC1403") != 0) {
        fprintf(stderr, "lcc2: unsupported target '%s'\n", target);
        return 2;
    }
    if (target != NULL) {
        (void)snprintf(target_directory, sizeof(target_directory),
                       "targets/%s", target);
    }
    source = read_source(input);
    if (source == NULL) {
        return 1;
    }
    arena = ast_arena_create();
    ctx = codegen_context_create();
    if (arena == NULL || ctx == NULL) {
        fprintf(stderr, "lcc2: out of memory\n");
        free(source);
        ast_arena_destroy(arena);
        codegen_context_destroy(ctx);
        return 1;
    }
    if (lcc2_parse_string(arena, source, &unit) != 0) {
        fprintf(stderr, "lcc2: cannot parse '%s'\n", input);
        goto cleanup;
    }
    if (codegen_program(ctx, unit) != 0) {
        fprintf(stderr, "lcc2: %s\n", codegen_error(ctx));
        goto cleanup;
    }
    for (runtime_index = 0;
         runtime_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
         ++runtime_index) {
        needed[runtime_index] =
            codegen_external_function_used(ctx, runtime_modules[runtime_index].name);
    }
    for (runtime_index = 0;
         runtime_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
         ++runtime_index) {
        const char *dependency = runtime_modules[runtime_index].dependency;
        size_t dependency_index;
        if (!needed[runtime_index] || dependency == NULL ||
            function_defined(unit, dependency)) {
            continue;
        }
        for (dependency_index = 0;
             dependency_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
             ++dependency_index) {
            if (strcmp(runtime_modules[dependency_index].name, dependency) == 0) {
                needed[dependency_index] = 1;
                break;
            }
        }
    }
    for (runtime_index = 0;
         runtime_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
         ++runtime_index) {
        const char *directory = runtime_modules[runtime_index].directory;
        if (directory == NULL) {
            if (target == NULL) {
                continue;
            }
            directory = target_directory;
        }
        if (needed[runtime_index]) {
            runtimes[runtime_index] =
                read_runtime(directory, runtime_modules[runtime_index].name);
            if (runtimes[runtime_index] == NULL) {
                goto cleanup;
            }
        }
    }
    file = fopen(output, "wb");
    if (file == NULL) {
        fprintf(stderr, "lcc2: cannot open '%s': %s\n", output, strerror(errno));
        goto cleanup;
    }
    {
        int failed = fputs(codegen_output(ctx), file) == EOF;
        for (runtime_index = 0;
             !failed && runtime_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
             ++runtime_index) {
            if (runtimes[runtime_index] != NULL) {
                failed = fputs(runtimes[runtime_index], file) == EOF;
            }
        }
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
    for (runtime_index = 0;
         runtime_index < sizeof(runtime_modules) / sizeof(runtime_modules[0]);
         ++runtime_index) {
        free(runtimes[runtime_index]);
    }
    free(source);
    return status;
}
