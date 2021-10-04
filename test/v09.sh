# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "oghi jkl"
echo -n 'F yeP'
echo -n 'k$p'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc def jkl" >&2
echo    "ghi jkl jkl" >&2
