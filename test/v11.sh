# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "oghi"
echo    "!krev"
sleep .1
echo    ""
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "fed" >&2
echo    "ihg" >&2
