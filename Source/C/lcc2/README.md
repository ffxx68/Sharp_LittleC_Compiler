# lcc2

`lcc2` is the new C implementation of the SC61860 compiler. It is being built
incrementally alongside the existing Pascal compiler, which remains the
behavioral reference.

## Build

From this directory, run:

```sh
make
```

The build requires GCC, Flex, Bison, and GNU Make, and produces `lcc2.exe`.
The initial project target builds the command-line scaffold and AST arena;
source compilation is not available until the frontend and backend milestones
are implemented.

Run the foundational checks with `make test`. Remove generated build files with
`make clean`.

## Layout

- `ast.h` defines the typed AST contract and arena allocation API.
- `ast.c` implements zero-initialized arena allocations and string copies.
- `main.c` provides the initial command-line entry point.
- `lexer.l`, `parser.y`, and the compiler subsystems are added in their
  respective Phase 5.3 milestones.
