# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "ghi"
echo    "."
echo    ":1,2yank"
echo    ":1put"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "abc" >&2
echo    "def" >&2
echo    "def" >&2
echo    "ghi" >&2
