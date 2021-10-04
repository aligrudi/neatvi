# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "oghi"
echo    "?abc"
echo -n "i1"
echo    "/ghi"
echo -n "i2"
echo    ":w"
echo    ":q"

# the expected output
echo    "1abc" >&2
echo    "def" >&2
echo    "2ghi" >&2
