# vi commands
echo    ":e $1"
echo    ":a"
echo    "a"
echo    "b"
echo    "c"
echo    "g/^[bg]/d"
echo    "."
echo    ":4y a"
echo    ":@a"
echo    ":wq"

# the expected output
echo    "a" >&2
echo    "c" >&2
