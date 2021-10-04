# vi commands
echo    ":i"
echo    "abc def"
echo    "."
echo    ":w $1"
echo    ":q"

# the expected output
echo    "abc def" >&2
