# vi commands
echo    ":e $1"
echo -n "iabc"
echo -n "odef"
echo -n "oabc"
echo    ":%s/abc/ghi/"
echo    ":w"
echo    ":q"

# the expected output
echo    "ghi" >&2
echo    "def" >&2
echo    "ghi" >&2
