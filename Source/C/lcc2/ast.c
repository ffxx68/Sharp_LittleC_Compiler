#include "ast.h"

#include <stdlib.h>
#include <string.h>

typedef struct AstArenaAllocation {
    void *memory;
    struct AstArenaAllocation *next;
} AstArenaAllocation;

struct AstArena {
    AstArenaAllocation *allocations;
};

AstArena *ast_arena_create(void)
{
    return (AstArena *)calloc(1, sizeof(AstArena));
}

void ast_arena_destroy(AstArena *arena)
{
    AstArenaAllocation *allocation;

    if (arena == NULL) {
        return;
    }

    allocation = arena->allocations;
    while (allocation != NULL) {
        AstArenaAllocation *next = allocation->next;

        free(allocation->memory);
        free(allocation);
        allocation = next;
    }

    free(arena);
}

void *ast_arena_alloc(AstArena *arena, size_t size)
{
    AstArenaAllocation *allocation;
    void *memory;

    if (arena == NULL) {
        return NULL;
    }

    memory = calloc(1, size == 0 ? 1 : size);
    if (memory == NULL) {
        return NULL;
    }

    allocation = (AstArenaAllocation *)malloc(sizeof(AstArenaAllocation));
    if (allocation == NULL) {
        free(memory);
        return NULL;
    }

    allocation->memory = memory;
    allocation->next = arena->allocations;
    arena->allocations = allocation;
    return memory;
}

char *ast_arena_strndup(AstArena *arena, const char *text, size_t length)
{
    size_t text_length;
    char *copy;

    if (arena == NULL || text == NULL) {
        return NULL;
    }

    text_length = 0;
    while (text_length < length && text[text_length] != '\0') {
        ++text_length;
    }
    if (text_length == (size_t)-1) {
        return NULL;
    }

    copy = (char *)ast_arena_alloc(arena, text_length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, text_length);
    copy[text_length] = '\0';
    return copy;
}

char *ast_arena_strdup(AstArena *arena, const char *text)
{
    if (text == NULL) {
        return NULL;
    }

    return ast_arena_strndup(arena, text, strlen(text));
}
