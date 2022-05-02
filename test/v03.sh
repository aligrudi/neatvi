# vi commands
echo    ":e $1"
printf	"iabc def"
printf	"0dB"
printf	"d0"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def" >&2
