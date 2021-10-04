# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "0dB"
echo -n "d0"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def" >&2
