# vi commands
echo    ":e $1"
echo    "iabc"
echo    "odef"
echo    "oghi"
echo    ":%g/def/d"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "ghi" >&2
