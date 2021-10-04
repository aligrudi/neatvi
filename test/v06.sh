# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "oghi jkl"
echo -n "omno pqr"
echo -n '1Gf d^$'
echo -n '+dfi'
echo -n '+f d$'
echo    ":w"
echo    ":q"

# the expected output
echo    " def" >&2
echo    " jkl" >&2
echo    "mno" >&2
