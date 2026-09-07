# ex commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "ghi"
echo    "."
echo    ':g/^a|^g/d'
echo    ":wq"

# the expected output
echo    "def" >&2
