# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"oghi"
echo    "!krev"
sleep .1
echo    ""
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "fed" >&2
echo    "ihg" >&2
