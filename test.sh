#!/bin/sh

for x in test/e??.sh; do
	rm -f /tmp/.neatvi[12]
	sh $x /tmp/.neatvi2 2>/tmp/.neatvi1 | ./vi -s -e >/dev/null
	cmp -s /tmp/.neatvi[12] || echo "Failed: $x"
	cmp -s /tmp/.neatvi[12] || diff -u /tmp/.neatvi[12]
done

for x in test/v??.sh; do
	rm -f /tmp/.neatvi[12]
	sh $x /tmp/.neatvi2 2>/tmp/.neatvi1 | ./vi -v >/dev/null
	cmp -s /tmp/.neatvi[12] || echo "Failed: $x"
	cmp -s /tmp/.neatvi[12] || diff -u /tmp/.neatvi[12]
done
