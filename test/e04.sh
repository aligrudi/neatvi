# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc def"
echo    "."
echo    ":a"
echo    "ghi jkl"
echo    "."
echo    ":undo"
echo    ":redo"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def" >&2
echo    "ghi jkl" >&2
