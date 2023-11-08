# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	"omno pqr"
printf	"1Gr1"
printf	"2Gfh2r2"
printf	"3Gfp4r3"
echo    ":w"
echo    ":q"

# the expected output
echo    "1bc def" >&2
echo    "g22 jkl" >&2
echo    "mno pqr" >&2
