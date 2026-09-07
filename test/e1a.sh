# ex commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    ""
echo    "def"
echo    "."
echo    ':g/^(d|$)/d'
echo    ":wq"

# the expected output
echo    "abc" >&2
