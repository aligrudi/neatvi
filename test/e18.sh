# ex commands
echo    ":e $1"
echo    ":a"
echo    "abc abc"
echo    "."
echo    ":s/(.*)? abc/def/"
echo    ":wq"

# the expected output
echo    "def" >&2
