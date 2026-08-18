# vi commands
echo    ":e $1"
echo    "ifdrD"
echo    "abc def"
echo    "ghi jkl"
echo    "1G\"ay$"
echo    "2G@a"
echo    "3G@a"
echo    ":wq"

# the expected output
echo    "fdrD" >&2
echo    "abc Def" >&2
echo    "ghi jkl" >&2
