#!/bin/sh

# Usage:
#   ./scripts/gen-asm-offsets.sh asm-offsets.s > asm-offsets.h

infile="$1"

if [ -z "$infile" ]; then
	echo "usage: $0 <asm-offsets.s>" >&2
	exit 1
fi

echo "/* Auto generated. Do not edit. */"
echo "#ifndef __ASM_OFFSETS_H__"
echo "#define __ASM_OFFSETS_H__"
echo

sed -n 's/^.*->\([A-Za-z0-9_]*\)[[:space:]]*\([0-9-]*\).*$/#define \1 \2/p' "$infile"

echo
echo "#endif /* __ASM_OFFSETS_H__ */"