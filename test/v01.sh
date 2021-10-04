#!/bin/sh

# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "hhA def"
echo -n "Ighi "
echo    ":w"
echo    ":q"

# the expected output
echo    "ghi abc def" >&2
