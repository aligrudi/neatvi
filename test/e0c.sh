# vi commands
echo    "e $1"
echo    "a"
echo    "x"
echo    "1"
echo    "2"
echo    "3"
echo    "4"
echo    "5"
echo    "."
echo    "1d"
echo    "1,4g/./+1put"
echo    "wq"

# the expected output
echo    "1" >&2
echo    "2" >&2
echo    "x" >&2
echo    "x" >&2
echo    "3" >&2
echo    "4" >&2
echo    "x" >&2
echo    "x" >&2
echo    "5" >&2
