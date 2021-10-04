# vi commands
echo    ":e $1"
echo    ":set ic"
echo -n "iABC"
echo -n "oDEF"
echo -n "oGHI"
echo    '1G/def'
echo -n 'i1'
echo    ":set noic"
echo    '/ghi'
echo -n 'i2'
echo    ":w"
echo    ":q"

# the expected output
echo    "ABC" >&2
echo    "21DEF" >&2
echo    "GHI" >&2
