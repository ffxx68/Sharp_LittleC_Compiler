#!/bin/sh
set -eu

dir=build/cli-tests
mkdir -p "$dir"
trap 'rm -f "$dir/source.c" "$dir/output.asm" "$dir/output.bin" "$dir/expected.asm" "$dir/error.txt"; rmdir "$dir"' EXIT

./lcc2.exe tests/pc1403_smoke.c -o "$dir/output.asm"
grep -q '^.ORG.*57392' "$dir/output.asm"
grep -q 'CALL.*main' "$dir/output.asm"
cp "$dir/output.asm" "$dir/expected.asm"

expect_failure()
{
    if ./lcc2.exe "$@" > /dev/null 2> "$dir/error.txt"; then
        echo "Unexpected CLI success: $*" >&2
        exit 1
    fi
    test -s "$dir/error.txt"
    cmp "$dir/output.asm" "$dir/expected.asm"
}

expect_failure
expect_failure tests/pc1403_smoke.c
expect_failure --unknown
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
printf 'void main(void) {}\000ignored\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
./lcc2.exe tests/pc1403_scalars.c -o "$dir/output.asm"
grep -q 'LIDP.*59391' "$dir/output.asm"
./lcc2.exe tests/pc1403_calls.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe tests/pc1403_global_init.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
