#!/bin/sh

# vi commands
echo    ":e $1"
echo -n "iabc"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
