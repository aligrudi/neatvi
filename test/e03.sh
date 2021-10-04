# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "."
echo    ":a"
echo    "def"
echo    "."
echo    ":undo"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
