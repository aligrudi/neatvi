# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	"omno pqr"
printf	'1Gf d^$'
printf	'+dfi'
printf	'+f d$'
echo    ":w"
echo    ":q"

# the expected output
echo    " def" >&2
echo    " jkl" >&2
echo    "mno" >&2
