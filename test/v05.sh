# vi commands
echo    ":e $1"
echo -n "iabc def"
echo -n "oghi jkl"
echo -n '1G$d$'
echo -n '2GdB'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc ghi jkl" >&2
