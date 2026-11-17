#!/bin/sh
set -eu

LCC2=${LCC2:-"$(pwd)/lcc2"}
PASM=${PASM:-../pasm/pasm}
dir=build/pc1350-rom-tests
mkdir -p "$dir"
trap 'rm -f "$dir/source.c" "$dir/preprocessed.c" "$dir/output.asm" "$dir/output.bin" "$dir/expected.asm" "$dir/error.txt"; rmdir "$dir"' EXIT

for routine in pc1350_str pc1350_val pc1350_compare; do
    case "$routine" in
        pc1350_str) call='pc1350_str(&number[0])' ;;
        pc1350_val) call='pc1350_val(3, &number[0])' ;;
        pc1350_compare) call='pc1350_compare(PC1350_COMPARE_EQ, 1, 1)' ;;
    esac
    printf '#include "pc1350.h"\n#include "pc1350.h"\n__xram __at(0x6800) unsigned char number[8];\nint main(void) { %s; return %s; }\n' "$call" "$call" > "$dir/source.c"
    gcc -E -P -nostdinc -Iinclude "$dir/source.c" -o "$dir/preprocessed.c"
    "$LCC2" --target PC1350 "$dir/preprocessed.c" -o "$dir/output.asm"
    for candidate in pc1350_str pc1350_val pc1350_compare; do
        if test "$candidate" = "$routine"; then
            test "$(grep -c "^$candidate:" "$dir/output.asm")" = 1
        else
            test "$(grep -c "^$candidate:" "$dir/output.asm")" = 0
        fi
    done
    test "$(grep -c '^PC1350_ROM_BEGIN:' "$dir/output.asm")" = 1
    test "$(grep -c '^PC1350_ROM_SAVED_IRAM:' "$dir/output.asm")" = 1
    grep -q 'LIDP[[:space:]]*0xFFF0' "$dir/output.asm"
    grep -q 'CPIA[[:space:]]*0xCE' "$dir/output.asm"
    grep -q 'CPIA[[:space:]]*0x03' "$dir/output.asm"
    if grep -Eq '^int2str:|^strcpy:|^strcat:' "$dir/output.asm"; then
        echo "PC1350 ROM strings selected a portable conversion/string runtime" >&2
        exit 1
    fi
    "$PASM" "$dir/output.asm" "$dir/output.bin"
done

gcc -E -P -nostdinc -Iinclude tests/pc1350_rom.c -o "$dir/preprocessed.c"
"$LCC2" --target PC1350 "$dir/preprocessed.c" -o "$dir/output.asm"
for routine in pc1350_str pc1350_val pc1350_compare PC1350_ROM_BEGIN; do
    test "$(grep -c "^$routine:" "$dir/output.asm")" = 1
done
"$PASM" "$dir/output.asm" "$dir/output.bin"
cp "$dir/output.asm" "$dir/expected.asm"
if "$LCC2" --target PC1401 "$dir/preprocessed.c" -o "$dir/output.asm" > /dev/null 2> "$dir/error.txt"; then
    echo "PC1350 ROM strings were accepted for PC1401" >&2
    exit 1
fi
test -s "$dir/error.txt"
cmp "$dir/output.asm" "$dir/expected.asm"

for routine in pc1350_str pc1350_val pc1350_compare; do
    case "$routine" in
        pc1350_str)
            parameters='const unsigned char *number'
            call='pc1350_str(number)' ;;
        pc1350_val)
            parameters='unsigned char length, unsigned char *number'
            call='pc1350_val(3, number)' ;;
        pc1350_compare)
            parameters='unsigned char operation, unsigned char left_length, unsigned char right_length'
            call='pc1350_compare(PC1350_COMPARE_EQ, 1, 1)' ;;
    esac
    printf '#include "pc1350.h"\nint %s(%s) { return 0; }\nint main(void) { unsigned char *number = 0; return %s; }\n' "$routine" "$parameters" "$call" > "$dir/source.c"
    gcc -E -P -nostdinc -Iinclude "$dir/source.c" -o "$dir/preprocessed.c"
    "$LCC2" --target PC1350 "$dir/preprocessed.c" -o "$dir/output.asm"
    if grep -q '^PC1350_ROM_' "$dir/output.asm"; then
        echo "User-defined $routine selected the ROM adapter" >&2
        exit 1
    fi
    "$PASM" "$dir/output.asm" "$dir/output.bin"
done
echo "PASS: PC1350 ROM string headers, selective inclusion and assembly"
