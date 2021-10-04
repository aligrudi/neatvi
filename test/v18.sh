# vi commands
echo    ":e $1"
echo    "iabc"
echo    "o"
echo    "Adef"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "def" >&2
