# ex commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "."
echo    "1y x"
echo    ":rx x rev"
echo    ":put x"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "cba" >&2
