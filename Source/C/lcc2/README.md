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
The initial project target builds the command-line scaffold, AST arena, and
Flex scanner. Source compilation is not available until the parser and backend
milestones are implemented.

Run the foundational checks with `make test`. Remove generated build files with
`make clean`.

## Layout

- `ast.h` defines the typed AST contract and arena allocation API.
- `ast.c` implements zero-initialized arena allocations and string copies.
- `lexer.l` and `lexer_tokens.h` define the scanner, token codes, and source
  locations. Typedef-name classification is deferred until parser scope data
  is available.
- `main.c` provides the initial command-line entry point.
- `parser.y` and the remaining compiler subsystems are added in their
  respective Phase 5.3 milestones.
