#!/bin/sh
# This script executes Neatvi's tests.

export EXINIT=""

# testcase vi_options test.sh
testcase() {
	rm -f /tmp/.neatvi[12]
	printf "$2: "
	sh $2 /tmp/.neatvi2 2>/tmp/.neatvi1 | ./vi $1 >/dev/null
	if ! cmp -s /tmp/.neatvi[12]; then
		printf "Failed\n"
		diff -u /tmp/.neatvi[12]
		exit 1
	fi
	printf "OK\n"
}

for x in test/e??.sh; do
	testcase "-s -e" "$x"
done
for x in test/v??.sh; do
	testcase "-v" "$x"
done
