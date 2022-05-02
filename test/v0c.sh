# vi commands
echo    ":e $1"
printf	"iabc def ghi"
echo    "?abc"
printf	"i1"
echo    "/ghi"
printf	"i2"
echo    ":w"
echo    ":q"

# the expected output
echo    "1abc def 2ghi" >&2
