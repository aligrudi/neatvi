# vi commands
echo    ":e $1"
echo    "iaa"
echo    "oab"
echo    "obb"
echo    ":g/a/.g/b/d"
echo    ":wq"

# the expected output
echo    "aa" >&2
echo    "bb" >&2
