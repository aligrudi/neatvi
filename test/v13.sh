# vi commands
echo    ":e $1"
echo    ":set ic"
printf	"iABC"
printf	"oDEF"
printf	"oGHI"
echo    '1G/def'
printf	"i1"
echo    ":set noic"
echo    '/ghi'
printf	"i2"
echo    ":w"
echo    ":q"

# the expected output
echo    "ABC" >&2
echo    "21DEF" >&2
echo    "GHI" >&2
