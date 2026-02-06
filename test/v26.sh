# vi commands
echo    ":e $1"
echo    ":set ai"
echo    "iabc"
echo    "def"
echo    "O123"
echo    "1Go456"
echo    "4Go789"
echo    ":wq"

# the expected output
echo    "abc" >&2
echo    "456" >&2
echo    "	123" >&2
echo    "	def" >&2
echo    "	789" >&2
