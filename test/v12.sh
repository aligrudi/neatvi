# vi commands
echo    ":e $1"
printf	"ia"
printf	"oab"
printf	"oabc"
printf	"0kklllji1"
printf	"kllji2"
printf	'$jx'
echo    ":w"
echo    ":q"

# the expected output
echo    "a" >&2
echo    "21ab" >&2
echo    "ab" >&2
