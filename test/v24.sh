# vi commands
echo    ":e $1"
echo    "iabc/def/ghi"
echo    "oabc/def/ghi"
echo    "oabc/0cedef"
echo    "o/abc"
echo    ":wq"

# the expected output
echo    "abc/def/" >&2
echo    "abc/" >&2
echo    "/" >&2
echo    "/" >&2
