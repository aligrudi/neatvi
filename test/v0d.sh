# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "oghi jkl"
echo -n "omno pqr"
echo    "1Gd/jkl/0"
echo    ":w"
echo    ":q"

# the expected output
echo    "mno pqr" >&2
