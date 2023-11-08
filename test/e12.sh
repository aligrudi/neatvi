# vi commands
echo    ":e $1"
echo    ":a"
echo    "a"
echo    "b"
echo    "c"
echo    "."
echo    ":w"
echo    ":e +2 $1"
echo    ':s/.*/z/'
echo    ":w"
echo    ":q"

# the expected output
echo    "a" >&2
echo    "z" >&2
echo    "c" >&2
