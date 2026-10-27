#!/bin/sh
set -eu

dir=build/cli-tests
mkdir -p "$dir"
trap 'rm -f "$dir/source.c" "$dir/output.asm" "$dir/output.bin" "$dir/expected.asm" "$dir/error.txt"; rmdir "$dir"' EXIT

compile()
{
    ./lcc2.exe --target PC1403 "$@"
}

    ./lcc2.exe tests/pc1403_smoke.c -o "$dir/output.asm"
grep -q '^.ORG.*57392' "$dir/output.asm"
grep -q 'CALL.*main' "$dir/output.asm"
if grep -q '^getchar:' "$dir/output.asm"; then
    echo "Unused target runtime was emitted" >&2
    exit 1
fi
cp "$dir/output.asm" "$dir/expected.asm"
compile tests/pc1403_smoke.c -o "$dir/output.asm"
cmp "$dir/output.asm" "$dir/expected.asm"

expect_failure()
{
    if compile "$@" > /dev/null 2> "$dir/error.txt"; then
        echo "Unexpected CLI success: $*" >&2
        exit 1
    fi
    test -s "$dir/error.txt"
    cmp "$dir/output.asm" "$dir/expected.asm"
}

expect_failure
expect_failure tests/pc1403_smoke.c
expect_failure --unknown
if ./lcc2.exe --target UNKNOWN tests/pc1403_smoke.c -o "$dir/output.asm" \
    > /dev/null 2> "$dir/error.txt"; then
    echo "Unexpected CLI success for an unsupported target" >&2
    exit 1
fi
expect_failure "$dir/missing.c" -o "$dir/output.asm"
expect_failure tests/pc1403_smoke.c -o "$dir/missing/output.asm"
printf 'int main(void) { return ; broken }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int value; void main(void) {}\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int __at(8) value = other; void main(void) {}\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int __at(8) value = { 1 }; void main(void) {}\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int __at(8) value = 1.5; void main(void) {}\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf '#pragma pc(57392)\nvoid main(void) {}\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { return 1 ? 2 : 3; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'enum color { RED, GREEN, BLUE } shade; int main(void) { return 0; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'void cpq1(void) {} unsigned int main(void) { unsigned int left = 1; unsigned int right = 2; return left == right; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'void main(void) {}\000ignored\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { int a[2]; return a[2]; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { int a[2] = {1, 2, 3}; return 0; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { const int a[2] = {1}; int i = 0; a[i] = 2; return 0; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'typedef unsigned char byte; int main(void) { byte value; return 0; }\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
compile tests/pc1403_scalars.c -o "$dir/output.asm"
grep -q 'LIDP.*59391' "$dir/output.asm"
compile tests/pc1403_calls.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
assert_no_runtime()
{
    if grep -Eq '^(getchar:|putchar:|PC1403_STDOUT_)' "$dir/output.asm"; then
        echo "Uncalled stdio runtime was emitted" >&2
        exit 1
    fi
}

printf 'int getchar(void); int putchar(int); void main(void) {}\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
assert_no_runtime
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
printf 'int getchar(void); int main(void) { return getchar(); }\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
test "$(grep -c '^getchar:' "$dir/output.asm")" = 1
if grep -Eq '^(putchar:|PC1403_STDOUT_)' "$dir/output.asm"; then
    echo "getchar pulled in unused output runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
printf 'int putchar(int); int main(void) { putchar(65); return putchar(66); }\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
test "$(grep -c '^putchar:' "$dir/output.asm")" = 1
test "$(grep -c '^PC1403_STDOUT_BUFFER:' "$dir/output.asm")" = 1
if grep -q '^getchar:' "$dir/output.asm"; then
    echo "putchar pulled in unused input runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe "$dir/source.c" -o "$dir/output.asm"
assert_no_runtime
printf 'int getchar(void) { return 7; } int putchar(int c) { return c; } int main(void) { return putchar(getchar()); }\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
test "$(grep -c '^getchar:' "$dir/output.asm")" = 1
test "$(grep -c '^putchar:' "$dir/output.asm")" = 1
if grep -Eq '^(PC1403_STDOUT_|PC1403_PUTCHAR_ERROR:)' "$dir/output.asm"; then
    echo "User-defined functions pulled in target runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
printf 'int getchar(void); int putchar(int); int main(void) { putchar(getchar()); return 0; }\n' > "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
grep -q '^getchar:' "$dir/output.asm"
grep -q 'CALL.*key_wait' "$dir/output.asm"
grep -q '^key_wait:' "$dir/output.asm"
grep -q 'LIDP.*0xFF5E' "$dir/output.asm"
if grep -q 'PC1403_GETCHAR_' "$dir/output.asm"; then
    echo "getchar unexpectedly contains key-code translation" >&2
    exit 1
fi
grep -q '^putchar:' "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
gcc -E -P -nostdinc -Iinclude tests/pc1403_stdio.c -o "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
gcc -E -P -nostdinc -Iinclude tests/pc1403_stdio_append.c -o "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
gcc -E -P -nostdinc -Iinclude tests/pc1403_getchar.c -o "$dir/source.c"
compile "$dir/source.c" -o "$dir/output.asm"
test "$(grep -c '^getchar:' "$dir/output.asm")" = 1
if grep -Eq '^(putchar:|PC1403_STDOUT_)' "$dir/output.asm"; then
    echo "getchar fixture pulled in unused output runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_comparisons.c -o "$dir/output.asm"
for name in CPE16 CPNE16 CPS16 CPSE16 CPG16 CPGE16; do
    test "$(grep -c "^LIB_$name:" "$dir/output.asm")" = 1
done
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_global_init.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_arrays.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
test "$(wc -c < "$dir/output.bin")" -le 1964
compile tests/pc1403_arrays_wide.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
test "$(wc -c < "$dir/output.bin")" -le 700
compile tests/pc1403_pointers.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_struct_members.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_bitwise_unary.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_shifts.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_logical.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_muldiv.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
test "$(wc -c < "$dir/output.bin")" -le 1964
compile tests/pc1403_switch.c -o "$dir/output.asm"
test "$(grep -c '^[[:space:]]*CASE$' "$dir/output.asm")" = 10
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
compile tests/pc1403_program.c -o "$dir/output.asm"
cp "$dir/output.asm" "$dir/expected.asm"
compile tests/pc1403_program.c -o "$dir/output.asm"
cmp "$dir/output.asm" "$dir/expected.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
printf 'void CPQ1(void) {} int main(void) { unsigned int left = 1; unsigned int right = 2; return left == right; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
