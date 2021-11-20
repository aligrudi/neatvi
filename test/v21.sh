# vi commands
echo    ":e $1"
echo    "i"
echo    "oa"
echo    ":%s/$/x/g"
echo    ":wq"

# the expected output
echo    "x" >&2
echo    "ax" >&2
