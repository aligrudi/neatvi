# vi commands
echo    ":e $1"
echo    "iabc def"
echo    "abc def"
echo    "1G/def/;/abc/;/def/"
echo    "rD"
echo    ":wq"

# the expected output
echo    "abc def" >&2
echo    "abc Def" >&2
