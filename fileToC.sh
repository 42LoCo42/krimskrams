#!/usr/bin/env bash
(($# < 1)) && {
	echo "Usage: $0 <file> [string name]"
	exit 1
}
file="$1"
name="$2"
(($# < 2)) && {
	name="${file%.*}"
}

echo "const char ${name}[] = {"
hexdump -bv "$file" \
| head -n-1 \
| cut -b 8- \
| sed -E '
	s|\s*$||;
	s| ([^0])| 0\1|g;
	s|(.) |\1, |g;
	s|$|,|;
'
echo "0};"
