# vi commands
echo    "e $1"
echo    "a"
echo    "1"
echo    "2"
echo    "3"
echo    "4"
echo    "5"
echo    "."
echo    "1,4g/./+1s/$/x/"
echo    "wq"

# the expected output
echo    "1" >&2
echo    "2x" >&2
echo    "3x" >&2
echo    "4x" >&2
echo    "5x" >&2
