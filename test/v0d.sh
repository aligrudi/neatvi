# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	"omno pqr"
echo    "1Gd/jkl/0"
echo    ":w"
echo    ":q"

# the expected output
echo    "mno pqr" >&2
