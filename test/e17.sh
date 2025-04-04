# ex commands
echo    ":e $1"
echo    ":rs a"
echo    "rs b"
echo    "%"
echo    "."
echo    "."
echo    ":ra a"
echo    ":put b"
echo    ":wq"

# the expected output
echo    "$1" >&2
