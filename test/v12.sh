# vi commands
echo    ":e $1"
echo -n "ia"
echo -n "oab"
echo -n "oabc"
echo -n '0kklllji1'
echo -n 'kllji2'
echo -n '$jx'
echo    ":w"
echo    ":q"

# the expected output
echo    "a" >&2
echo    "21ab" >&2
echo    "ab" >&2
