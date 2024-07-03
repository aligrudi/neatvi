# vi commands
echo    ":e $1"
echo    ":a"
echo    "c"
echo    "b"
echo    "a"
echo    ""
echo    "a"
echo    "."
echo    "1G!}sort"
echo    ""
echo    ":wq"

# the expected output
echo    "a" >&2
echo    "b" >&2
echo    "c" >&2
echo    "" >&2
echo    "a" >&2
