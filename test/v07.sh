# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	'1Gyy'
printf	'pGp'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "abc" >&2
echo    "def" >&2
echo    "abc" >&2
