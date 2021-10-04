# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "oghi jkl"
echo -n "omno pqr"
echo -n "1Gr1"
echo -n "2Gfh2r2"
echo -n "3Gfp4r3"
echo    ":w"
echo    ":q"

# the expected output
echo    "1bc def" >&2
echo    "g22 jkl" >&2
echo    "mno pqr" >&2
