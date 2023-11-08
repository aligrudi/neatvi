# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	"F yeP"
printf	'k$p'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def jkl" >&2
echo    "ghi jkl jkl" >&2
