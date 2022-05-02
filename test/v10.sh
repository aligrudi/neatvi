# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"oghi"
printf	'"add'
printf	'dd'
printf	'"bdd'
printf	'P"ap"bp'
echo    ":4d"
echo    ":w"
echo    ":q"

# the expected output
echo    "def" >&2
echo    "ghi" >&2
echo    "abc" >&2
