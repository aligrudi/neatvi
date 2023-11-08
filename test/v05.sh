# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"oghi jkl"
printf	'1G$d$'
printf	'2GdB'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc ghi jkl" >&2
