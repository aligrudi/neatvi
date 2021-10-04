# vi commands
echo    ":e $1"
echo    "iabc"
echo    "odef"
echo    "oghi"
echo -n "1G"
echo    ":/ghi/d"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "def" >&2
