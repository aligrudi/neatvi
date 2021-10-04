# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "oghi"
echo -n '"add'
echo -n 'dd'
echo -n '"bdd'
echo -n 'P"ap"bp'
echo    ':4d'
echo    ":w"
echo    ":q"

# the expected output
echo    "def" >&2
echo    "ghi" >&2
echo    "abc" >&2
