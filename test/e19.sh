# ex commands
echo    ":e $1"
echo    ":a"
echo    "abc"
echo    "def"
echo    "ghi"
echo    "."
echo    ':1;1,/^def/-s/^./A/'
echo    ':3;?^def?+1s/^./G/'
echo    ":wq"

# the expected output
echo    "Abc" >&2
echo    "def" >&2
echo    "Ghi" >&2
