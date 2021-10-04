# vi commands
echo    ":e $1"
echo    ":a"
echo    "ab "
echo    " e "
echo    "."
echo    ':%s/ *$//g'
echo    ":w"
echo    ":q"

# the expected output
echo    "ab" >&2
echo    " e" >&2
