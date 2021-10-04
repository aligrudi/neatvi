# vi commands
echo    ":e $1"
echo    "iabc"
echo    "odef"
echo    "oghi"
echo    ":%g/^[dg]/s/h/x/"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "def" >&2
echo    "gxi" >&2
