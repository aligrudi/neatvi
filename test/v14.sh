# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "oghi"
echo -n '1GA123'
echo    'j.j.'
echo    ":w"
echo    ":q"

# the expected output
echo    "abc123" >&2
echo    "def123" >&2
echo    "ghi123" >&2
