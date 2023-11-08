#!/bin/sh

# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	"kOmno pqr"
echo    ":w"
echo    ":q"

# the expected output
echo    "mno pqr" >&2
echo    "abc def" >&2
echo    "ghi jkl" >&2
