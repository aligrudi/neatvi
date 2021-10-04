# vi commands
echo    ":e $1"
echo    "iabc"
echo    "odef"
echo    "oghi"
echo    "/^/"
echo    "NxnP/$/"
echo    "xNp"
echo    ":w"
echo    ":q"

# the expected output
echo    "abc" >&2
echo    "efi" >&2
echo    "dgh" >&2
