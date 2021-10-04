# vi commands
echo    ":e $1"
echo    ":set ai"
echo    "iabc"
echo    ""
echo    "def"
echo    ":wq"

# the expected output
echo    "	abc" >&2
echo    "" >&2
echo    "	def" >&2
