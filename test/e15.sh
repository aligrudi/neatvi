# ex commands
echo    ":e $1"
echo    ":a"
echo    "2s/abc/def/"
echo    "abc"
echo    "."
echo    ":w"
echo    ":so %"
echo    ":wq"

# the expected output
echo    "2s/abc/def/" >&2
echo    "def" >&2
