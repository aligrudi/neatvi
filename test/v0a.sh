# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "o"
echo -n "odef "
echo -n "oghi."
echo -n "ojkl"
echo -n '1G5J'
echo -n "i^"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def ghi.^  jkl" >&2
