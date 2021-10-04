# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n 'dd'
echo -n 'P'
echo    ":w"
echo    ":q"

# the expected output
echo    "def" >&2
echo    "abc" >&2
