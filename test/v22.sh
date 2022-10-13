# vi commands
echo    ":e $1"
echo    ":a"
echo    "a"
echo    ""
echo    "b"
echo    ""
echo    "."
echo    "1G>G"
echo    ":wq"

# the expected output
echo    "	a" >&2
echo    "" >&2
echo    "	b" >&2
echo    "" >&2
