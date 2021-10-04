# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "axy"
echo    "."
echo    ':%s/a(..)/\\1a/g'
echo    ":w"
echo    ":q"

# the expected output
echo    "bca" >&2
echo    "xya" >&2
