# vi commands
echo    ":e $1"
echo    "iabc"
echo    "odef"
echo    "oghi"
echo -n "1G''x"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "def" >&2
echo    "hi" >&2
