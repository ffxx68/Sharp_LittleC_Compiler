#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    fprintf(stream, "Usage: lcc2 --help | --version\n");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("lcc2 SC61860 C compiler prototype");
        return 0;
    }

    if (argc == 1) {
        print_usage(stderr);
        return 2;
    }

    fprintf(stderr, "lcc2: source compilation is not available yet\n");
    return 2;
}
