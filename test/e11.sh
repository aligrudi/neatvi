# vi commands
echo    ":e $1"
echo    ":a"
echo    "a"
echo    "ab"
echo    "."
echo    ':%s/^a$/x/'
echo    ":w"
echo    ":q"

# the expected output
echo    "x" >&2
echo    "ab" >&2
