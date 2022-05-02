# vi commands
echo    ":e $1"
printf	"iabc"
printf	"odef"
printf	"1Gfxr1"
printf	"dfx"
echo    "d/xyz/"
echo    ":w"
echo    ":q"

# the expected output
echo    "1bc" >&2
echo    "def" >&2
