#!/bin/sh

# vi commands
echo    ":e $1"
printf	"iabc"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
