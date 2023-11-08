# vi commands
echo    ":e $1"
printf	"iabc abc abc"
printf	"oabc abc abc"
printf	"oabc abc abc"
echo    ":2,3s/abc/111/"
echo    ":%s/abc/222/g"
echo    ":w"
echo    ":q"

# the expected output
echo    "222 222 222" >&2
echo    "111 222 222" >&2
echo    "111 222 222" >&2
