# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"oghi"
echo    "?abc"
printf	"i1"
echo    "/ghi"
printf	"i2"
echo    ":w"
echo    ":q"

# the expected output
echo    "1abc" >&2
echo    "def" >&2
echo    "2ghi" >&2
