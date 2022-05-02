# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"oghi"
printf	"1GA123"
echo    'j.j.'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc123" >&2
echo    "def123" >&2
echo    "ghi123" >&2
