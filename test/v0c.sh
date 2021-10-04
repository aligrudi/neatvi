# vi commands
echo    ":e $1"
echo -n "iabc def ghi"
echo    "?abc"
echo -n "i1"
echo    "/ghi"
echo -n "i2"
echo    ":w"
echo    ":q"

# the expected output
echo    "1abc def 2ghi" >&2
