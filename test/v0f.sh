# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "1Gfxr1"
echo -n "dfx"
echo    "d/xyz/"
echo    ":w"
echo    ":q"

# the expected output
echo    "1bc" >&2
echo    "def" >&2
