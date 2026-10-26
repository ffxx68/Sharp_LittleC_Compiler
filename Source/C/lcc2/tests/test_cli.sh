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
printf 'void cpq1(void) {} unsigned int main(void) { return 1 == 2; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'void main(void) {}\000ignored\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { int a[2]; return a[2]; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { int a[2] = {1, 2, 3}; return 0; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
printf 'int main(void) { const int a[2] = {1}; int i = 0; a[i] = 2; return 0; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
./lcc2.exe tests/pc1403_scalars.c -o "$dir/output.asm"
grep -q 'LIDP.*59391' "$dir/output.asm"
./lcc2.exe tests/pc1403_calls.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe tests/pc1403_comparisons.c -o "$dir/output.asm"
for name in CPE16 CPNE16 CPS16 CPSE16 CPG16 CPGE16; do
    test "$(grep -c "^LIB_$name:" "$dir/output.asm")" = 1
done
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe tests/pc1403_global_init.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe tests/pc1403_arrays.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
test "$(wc -c < "$dir/output.bin")" -le 1964
./lcc2.exe tests/pc1403_arrays_wide.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
test "$(wc -c < "$dir/output.bin")" -le 700
./lcc2.exe tests/pc1403_pointers.c -o "$dir/output.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe tests/pc1403_program.c -o "$dir/output.asm"
cp "$dir/output.asm" "$dir/expected.asm"
./lcc2.exe tests/pc1403_program.c -o "$dir/output.asm"
cmp "$dir/output.asm" "$dir/expected.asm"
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
printf 'void CPQ1(void) {} int main(void) { return 1 == 2; }\n' > "$dir/source.c"
expect_failure "$dir/source.c" -o "$dir/output.asm"
