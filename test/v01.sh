#!/bin/sh

# vi commands
echo    ":e $1"
printf	"iabc"
printf	"hhA def"
printf	"Ighi "
echo    ":w"
echo    ":q"

# the expected output
echo    "ghi abc def" >&2
