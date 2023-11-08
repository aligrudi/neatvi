#!/bin/sh

# vi commands
echo    ":e $1"
echo    "iabc def"
printf	"ghi jkl"
printf	"1Gf cE 123"
printf	"2Gf cB456"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc 123" >&2
echo    "456 jkl" >&2
