# vi commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "."
echo    ':%s/(abc|def)/xyz/'
echo    ":w"
echo    ":q"

# the expected output
echo    "xyz" >&2
echo    "xyz" >&2
