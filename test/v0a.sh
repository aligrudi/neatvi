# vi commands
echo    ":e $1"
printf	"iabc"
printf	"o"
printf	"odef "
printf	"oghi."
printf	"ojkl"
printf	"1G5J"
printf	"i^"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def ghi.^  jkl" >&2
