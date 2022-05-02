# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"dd"
printf	"P"
echo    ":w"
echo    ":q"

# the expected output
echo    "def" >&2
echo    "abc" >&2
