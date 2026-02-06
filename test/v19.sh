# vi commands
echo    ":e $1"
echo    ":set ai"
echo    "iabc"
echo    ""
echo    "def"
echo    ""
echo    "ghi"
echo    ":wq"

# the expected output
echo    "	abc" >&2
echo    "" >&2
echo    "	def" >&2
echo    "" >&2
echo    "ghi" >&2
