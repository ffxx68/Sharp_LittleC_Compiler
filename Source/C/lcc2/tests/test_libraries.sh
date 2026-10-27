#!/bin/sh
set -eu

dir=build/library-tests
mkdir -p "$dir"
trap 'rm -f "$dir/source.c" "$dir/preprocessed.c" "$dir/output.asm" "$dir/output.bin" "$dir/expected.asm" "$dir/error.txt"; rmdir "$dir"' EXIT

preprocess()
{
    gcc -E -P -nostdinc -Iinclude "$dir/source.c" -o "$dir/preprocessed.c"
}

compile()
{
    preprocess
    ./lcc2.exe --target PC1403 "$dir/preprocessed.c" -o "$dir/output.asm"
}

assert_count()
{
    test "$(grep -c "^$1:" "$dir/output.asm" || true)" = "$2"
}

assert_no_runtime()
{
    if grep -Eq '^(getchar:|putchar:|puts:|int2str:|strcpy:|strcat:|PC1403_|LCC2_)' "$dir/output.asm"; then
        echo "Uncalled library runtime was emitted" >&2
        exit 1
    fi
}

printf '#include "stdint.h"\n#include "compat.h"\n#include "stdio.h"\n#include "string.h"\n#include "int2str.h"\n#include "lcd.h"\n#include "key.h"\n#include "stdint.h"\n#include "compat.h"\n#include "stdio.h"\n#include "string.h"\n#include "int2str.h"\n#include "lcd.h"\n#include "key.h"\nvoid main(void) { uint8_t a = 0x1234; uint16_t b = 65535; byte c = a; word d = b; }\n' > "$dir/source.c"
compile
assert_no_runtime
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"

printf '#include "int2str.h"\n__xram __at(0xE700) char buffer[6];\nvoid main(void) { int2str(0, &buffer[0]); int2str(65535, &buffer[0]); }\n' > "$dir/source.c"
compile
assert_count int2str 1
assert_count puts 0
assert_count putchar 0
assert_count getchar 0
assert_count strcpy 0
assert_count strcat 0
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
cp "$dir/output.asm" "$dir/expected.asm"
./lcc2.exe "$dir/preprocessed.c" -o "$dir/output.asm"
cmp "$dir/output.asm" "$dir/expected.asm"

printf '#include "stdio.h"\n__xram __at(0xE700) char text[4] = {65,66,67,0};\nvoid main(void) { puts(&text[0]); puts(&text[0]); }\n' > "$dir/source.c"
compile
assert_count puts 1
assert_count putchar 1
assert_count PC1403_STDOUT_BUFFER 1
assert_count getchar 0
assert_count int2str 0
assert_count strcpy 0
assert_count strcat 0
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
./lcc2.exe "$dir/preprocessed.c" -o "$dir/output.asm"
assert_count puts 1
assert_count putchar 0
assert_count PC1403_STDOUT_BUFFER 0

printf '#include "stdio.h"\n__xram __at(0xE700) char text[2] = {65,0};\nvoid main(void) { puts(&text[0]); putchar(66); }\n' > "$dir/source.c"
compile
assert_count puts 1
assert_count putchar 1
assert_count PC1403_STDOUT_BUFFER 1
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"

printf '#include "stdio.h"\nint putchar(int c) { return c; }\n__xram __at(0xE700) char text[2] = {65,0};\nvoid main(void) { puts(&text[0]); }\n' > "$dir/source.c"
compile
assert_count puts 1
assert_count putchar 1
assert_count PC1403_STDOUT_BUFFER 0
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
cp "$dir/output.asm" "$dir/expected.asm"
./lcc2.exe "$dir/preprocessed.c" -o "$dir/output.asm"
cmp "$dir/output.asm" "$dir/expected.asm"

printf '#include "stdio.h"\nint puts(const char *text) { return 7; }\n__xram __at(0xE700) char text;\nvoid main(void) { puts(&text); }\n' > "$dir/source.c"
compile
assert_count puts 1
assert_count putchar 0
if grep -q '^LCC2_PUTS_' "$dir/output.asm"; then
    echo "User puts selected library runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"

printf '#include "int2str.h"\nvoid int2str(unsigned int n, char *d) { *d = 0; }\n__xram __at(0xE700) char text;\nvoid main(void) { int2str(1, &text); }\n' > "$dir/source.c"
compile
assert_count int2str 1
if grep -q '^LCC2_INT2STR_' "$dir/output.asm"; then
    echo "User int2str selected library runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
cp "$dir/output.asm" "$dir/expected.asm"

expect_failure()
{
    if compile > /dev/null 2> "$dir/error.txt"; then
        echo "Unexpected library signature success" >&2
        exit 1
    fi
    test -s "$dir/error.txt"
    cmp "$dir/output.asm" "$dir/expected.asm"
}

printf '#include "stdio.h"\nvoid main(void) { puts(1); }\n' > "$dir/source.c"
expect_failure
printf '#include "stdio.h"\nvoid main(void) { getchar(1); }\n' > "$dir/source.c"
expect_failure
printf '#include "stdio.h"\nvoid main(void) { putchar(); }\n' > "$dir/source.c"
expect_failure
printf '#include "int2str.h"\nvoid reject(unsigned int n, const char *d) { int2str(n, d); }\nvoid main(void) {}\n' > "$dir/source.c"
expect_failure
printf '#include "int2str.h"\nvoid reject(char *d) { int2str(d, d); }\nvoid main(void) {}\n' > "$dir/source.c"
expect_failure

gcc -E -P -nostdinc -Iinclude tests/libraries.c -o build/pc1403_libraries.c
./lcc2.exe --target PC1403 build/pc1403_libraries.c -o build/pc1403_libraries.asm
../../../pasm.exe build/pc1403_libraries.asm build/pc1403_libraries.bin
test "$(wc -c < build/pc1403_libraries.bin)" -le 1232

for fixture in stdio stdio_append getchar; do
    gcc -E -P -nostdinc -Iinclude "tests/$fixture.c" -o "build/$fixture.c"
    ./lcc2.exe --target PC1403 "build/$fixture.c" -o "build/$fixture.asm"
    ../../../pasm.exe "build/$fixture.asm" "build/$fixture.bin"
done

gcc -E -P -nostdinc -Iinclude tests/pc1403_device.c -o build/pc1403_device.c
./lcc2.exe --target PC1403 build/pc1403_device.c -o build/pc1403_device.asm
cp build/pc1403_device.asm "$dir/output.asm"
for routine in lcd_on lcd_off lcd_cls lcd_pset lcd_hline key_wait key_scan keybrk; do
    assert_count "$routine" 1
done
assert_count PC1403_LCD_PSET_IMPL 1
../../../pasm.exe build/pc1403_device.asm build/pc1403_device.bin

printf '#include "key.h"\nunsigned char key_scan(void) { return 0xFF; }\nvoid main(void) { key_scan(); }\n' > "$dir/source.c"
compile
assert_count key_scan 1
if grep -q '^PC1403_KEY_SCAN_' "$dir/output.asm"; then
    echo "User key_scan selected the supplied target runtime" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"

printf '#include "lcd.h"\nvoid lcd_pset(unsigned char x, unsigned char y, unsigned char mode) { }\nvoid main(void) { lcd_hline(0, 0, 2, 1); }\n' > "$dir/source.c"
compile
assert_count lcd_pset 1
assert_count lcd_hline 1
assert_count PC1403_LCD_PSET_IMPL 1
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
echo "PASS: library headers, signatures, dependencies and selective inclusion"
