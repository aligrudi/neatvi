# ex commands
echo    ":e $1"
echo    ":rs x"
echo    "a"
echo    "b"
echo    "c"
echo    "."
echo    ":put x"
echo    ":wq"

# the expected output
echo    "a" >&2
echo    "b" >&2
echo    "c" >&2
