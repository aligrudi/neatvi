# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "ghi"
echo    "."
echo    ":2ka"
echo    ":'ad"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "ghi" >&2
