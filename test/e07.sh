# vi commands
echo    ":e $1"
echo    ":a"
echo    "<< >> << >>"
echo    "<< >> << >>"
echo    "."
echo    ":%s/<</left {/g"
echo    ":%s/>>/right }/g"
echo    ":w"
echo    ":q"

# the expected output
echo    "left { right } left { right }" >&2
echo    "left { right } left { right }" >&2
