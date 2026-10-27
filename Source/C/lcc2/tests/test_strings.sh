#!/bin/sh
set -eu

dir=build/string-tests
mkdir -p "$dir"
trap 'rm -f "$dir/source.c" "$dir/preprocessed.c" "$dir/output.asm" "$dir/output.bin" "$dir/expected.asm" "$dir/error.txt"; rmdir "$dir"' EXIT

compile()
{
    gcc -E -P -nostdinc -Iinclude "$dir/source.c" -o "$dir/preprocessed.c"
    ./lcc2.exe "$dir/preprocessed.c" -o "$dir/output.asm"
}

assert_count()
{
    test "$(grep -c "^$1:" "$dir/output.asm" || true)" = "$2"
}

assert_no_device_runtime()
{
    if grep -Eq '^(getchar:|putchar:|PC1403_)' "$dir/output.asm"; then
        echo "String calls pulled in device runtime" >&2
        exit 1
    fi
}

printf '#include "string.h"\n#include "string.h"\nvoid main(void) {}\n' > "$dir/source.c"
compile
assert_count strcpy 0
assert_count strcat 0
assert_no_device_runtime

for routine in strcpy strcat; do
    printf '#include "string.h"\n__xram __at(0xE600) char dest[8];\n__xram __at(0xE610) char src[3] = {65,66,0};\nvoid main(void) { %s(&dest[0], &src[0]); %s(&dest[0], &src[0]); }\n' \
        "$routine" "$routine" > "$dir/source.c"
    compile
    assert_count "$routine" 1
    if test "$routine" = strcpy; then
        assert_count strcat 0
        if grep -q '^LCC2_STRCAT_' "$dir/output.asm"; then exit 1; fi
    else
        assert_count strcpy 0
        if grep -q '^LCC2_STRCPY_' "$dir/output.asm"; then exit 1; fi
    fi
    assert_no_device_runtime
    ../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
    cp "$dir/output.asm" "$dir/expected.asm"
    ./lcc2.exe --target PC1403 "$dir/preprocessed.c" -o "$dir/output.asm"
    cmp "$dir/output.asm" "$dir/expected.asm"
done

printf '#include "string.h"\n__xram __at(0xE600) char dest[8];\n__xram __at(0xE610) char src[3] = {65,66,0};\nvoid main(void) { strcpy(strcat(&dest[0], &src[0]), &src[0]); }\n' > "$dir/source.c"
compile
assert_count strcpy 1
assert_count strcat 1
assert_no_device_runtime
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"

printf '#include "string.h"\nchar *strcpy(char *d, const char *s) { return d; }\nchar *strcat(char *d, const char *s) { return d; }\n__xram __at(0xE600) char dest;\nvoid main(void) { strcpy(&dest, &dest); strcat(&dest, &dest); }\n' > "$dir/source.c"
compile
assert_count strcpy 1
assert_count strcat 1
if grep -Eq '^LCC2_(STRCPY|STRCAT)_' "$dir/output.asm"; then
    echo "User-defined string functions pulled in library implementations" >&2
    exit 1
fi
../../../pasm.exe "$dir/output.asm" "$dir/output.bin"
cp "$dir/output.asm" "$dir/expected.asm"

expect_failure()
{
    if compile > /dev/null 2> "$dir/error.txt"; then
        echo "Unexpected string API success" >&2
        exit 1
    fi
    test -s "$dir/error.txt"
    cmp "$dir/output.asm" "$dir/expected.asm"
}

printf '#include "string.h"\n__xram __at(0xE600) char dest;\nvoid main(void) { strcpy(&dest, &dest, 3); }\n' > "$dir/source.c"
expect_failure
printf '#include "string.h"\nvoid reject(const char *dest, const char *src) { strcat(dest, src); }\nvoid main(void) {}\n' > "$dir/source.c"
expect_failure
printf '#include "string.h"\n__xram __at(0xE600) int dest;\nvoid main(void) { strcpy(&dest, &dest); }\n' > "$dir/source.c"
expect_failure
printf '#include "string.h"\n__xram __at(0xE600) char dest;\nvoid main(void) { strlen(&dest); }\n' > "$dir/source.c"
expect_failure

printf '#include "string.h"\n__xram __at(0xE600) char dest;\nvoid main(void) { strcpy(&dest, &dest); }\n' > "$dir/source.c"
gcc -E -P -nostdinc -Iinclude "$dir/source.c" -o "$dir/preprocessed.c"
if (cd "$dir"; ../../lcc2.exe preprocessed.c -o output.asm) \
    > /dev/null 2> "$dir/error.txt"; then
    echo "Missing string runtime was silently accepted" >&2
    exit 1
fi
grep -q "cannot find runtime 'strcpy'" "$dir/error.txt"
cmp "$dir/output.asm" "$dir/expected.asm"

gcc -E -P -nostdinc -Iinclude tests/strings.c -o build/pc1403_strings.c
./lcc2.exe build/pc1403_strings.c -o build/pc1403_strings.asm
../../../pasm.exe build/pc1403_strings.asm build/pc1403_strings.bin
test "$(wc -c < build/pc1403_strings.bin)" -le 1232
echo "PASS: string headers and selective runtime inclusion"
