# vi commands
echo    ":e $1"
echo    "iآبaä"
echo    ":wq"

# the expected output
echo    "آبä" >&2
