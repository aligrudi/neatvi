# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "."
printf  ':%%s/(b.*)$/(\\1)/g\n'
echo    ":w"
echo    ":q"

# the expected output
echo    "a(bc)" >&2
echo    "def" >&2
