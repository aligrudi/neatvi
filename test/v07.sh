# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n '1Gyy'
echo -n 'pGp'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "abc" >&2
echo    "def" >&2
echo    "abc" >&2
