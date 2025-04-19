NEATVI
Neatvi is a small vi/ex-style editor for editing UTF-8 text. It supports:

Syntax highlighting

Multiple windows

Right-to-left language support

Keymaps

CONFIGURATION
Edit conf.h to adjust:

Syntax highlighting rules

Text direction patterns

To define a new keymap:

Add an array in kmap.h (e.g., kmap_fa)

Add it to the kmaps array

The first entry of the new array defines its name

You can change the current keymap with the :cm command.
While in insert mode:

^e switches to the English keymap

^f switches to the alternate keymap (last set with :cm)

⚠️ VTE-based terminals (e.g., GNOME Terminal) implement a non-standard bidi rendering.
In such cases, set LNPREF in conf.h to "\33[8l".

COMMANDS
New ex commands (not in ex(1)):
text
Copy
Edit
:cm[ap][!] [kmap]     # Show or set the keymap
:ft [filetype]        # Show or set current file type
:ta[g] tag            # Jump to tag (TAGPATH or ./tags)
:tn[ext]              # Next matching tag
:tp[rev]              # Previous matching tag
:po[p]                # Pop tag stack
:b[uffer] [buf]       # Show/switch/delete/renumber buffers
:rs reg               # Read lines into yank buffer
:rx reg cmd           # Run cmd with buffer content as input/output
:rk reg path          # Communicate with UNIX socket
:ra reg               # Execute content of buffer, supports ^rx and ^vx substitutions
:ec[ho] msg           # Print message (for scripts)
New key mappings:
text
Copy
Edit
^a         : Search word under cursor (normal mode)
^p         : Insert default yank buffer (insert mode)
^rX        : Insert buffer X (insert mode)
z>, z<     : Change text direction
ze, zf     : Switch keymaps (normal mode)
gu/gU/g~   : Switch character case
^l         : Update terminal size
^], ^t     : Tag navigation
gf, gl     : Edit file under cursor (gl supports line/column)
^ws, ^wo, ^wc, ^wj, ^wk, ^wx  : Window management
^wgf, ^wgl, ^w^]              : Run file/jump commands in new window
zj, zk, zD : Buffer nav/delete
zJ, zK     : :next / :prev
q          : Quick access
^a         : Insert history in `:`/search/pipe prompts
VI DIFFERENCES
Uses POSIX EREs (regex) everywhere: search, conf.h, tags

= prefix in paths means relative to current file's directory

Highlights ls-named files as directory listings (gl edits selected file)

Extended yank buffers with names like \x

If EXINIT="so /path/to/exrc", it runs commands from file at startup

Example usage in an ex file:

text
Copy
Edit
rs a
:!git add %
.
OPTIONS
To improve performance, disable features with EXINIT:

sh
Copy
Edit
EXINIT="set noshape | set noorder | set nohl | set td=+2"
Supported options:

Option	Description
td	Text direction context:
+2 → always LTR	
+1 → conf.h or LTR	
-1 → conf.h or RTL	
-2 → always RTL	
0 → LTR if first char is single-byte, else uses conf.h	
shape	Arabic/Farsi letter shaping
order	Reorder characters per conf.h:
1 → reorder only if multibyte UTF-8 is present	
2 → reorder all lines	
hl	Enable syntax highlighting
hll	Highlight current line
lim	Max line length for reordering/highlighting
ru	Ruler redraw:
0 never, 1 always, 2 if multiple windows, 4 on file change	
hist	History size for :/search/pipe (0 = no history)
ai	Auto-indent
aw	Auto-write
ic	Ignore case
wa	Write any file
MARKS & BUFFERS
Special marks:
* → Previous change position

[ → First line of previous change

] → Last line of previous change

Special yank buffers:
/ → Last search

: → Last ex command

! → Last pipe command

% → Current filename

" → Default yank buffer

; → Current line

. → Last vi command

# → Cursor line number

^ → Cursor line offset

\/ → Search history

\:, \! → Ex/pipe history

QUICK ACCESS (q in normal mode)
Prints buffer list

Waits for a key:

Digit → switch to that buffer

Letter:

If extended buffer with that letter exists → execute its contents

Else → runs ECMD (from conf.h) with params:

Letter

Current filename

Line number

Cursor offset

ECMD output is executed as ex commands.

Example: Use q-commands for IDE features (e.g., LSP integration).
See ecmd.sh for a simple shell script version.

AUTO-COMPLETION
If hist is non-zero:

In : / / / ! prompts:

^a completes input using history

In insert mode:

^a:

Saves line up to cursor in buffer ~

Runs :ra \~ to execute \~ buffer

Second ^a inserts suggested text

Example \~ buffer:

text
Copy
Edit
rs \~
ec completing '�~'
rs ~
completion
�.
.
Real implementations can use rx or rk with external tools for completions.

