# vi commands
echo    ":e $1"
echo    "iabc def"
echo    "abc ghi"
echo    "1Gd/def/;/abc/-"
echo    ":wq"

# the expected output
echo    "abc ghi" >&2
