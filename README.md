NEATVI
======

Neatvi is a small vi/ex editor for editing UTF-8 text.  It supports
syntax highlighting, multiple windows, right-to-left languages, and
keymaps.

CONFIGURATION
-------------

Edit conf.h to adjust syntax highlighting rules and text direction
patterns.  To define a new keymap, create a new array in kmap.h, like
kmap_fa, and add it to kmaps array in the same header (the first entry
of the new array specifies its name).  The current keymap may be
changed with :cm ex command.  When in input mode, ^e activates the
English keymap and ^f switches to the alternate keymap (the last
keymap specified with :cm).

Sadly, VTE-based terminals such as GNOME's implement a
backward-incompatible extension of virtual terminal escape codes to
render bidirectional text by default.  When using these terminal, the
value of LNPREF macro in conf.h needs to be changed to "\33[8l".

COMMANDS
--------

Commands not available in ex(1):

`:cm[ap][!] [kmap]`  
    Without kmap, prints the current keymap name.  
    When kmap is specified, sets the alternate keymap to  
    kmap and, unless ! is given, switches to this keymap.  
`:ft [filetype]`  
    Without filetype, prints the current file type.  
    When filetype is specified, sets the file type of the  
    current ex buffer.  
`:ta[g] tag`  
    Jumps to tag (tags file: TAGPATH environment variable or ./tags).  
`:tn[ext]`  
    Jumps to the next matching tag.  
`:tp[rev]`  
    Jumps to the previous matching tag.  
`:po[p]`  
    Pops tag stack.  
`:b[uffer] [buf]`  
    Without buf, prints buffer list.  Switches to the given buffer  
    if buf is a buffer number or alias.  Also, buf can be -, +, !,  
    and ~ to switch to the previous buffer, switch to the next buffer,  
    delete the current buffer, and renumber buffers, respectively.  
`:rs reg`  
    Reads dot-terminated lines (similar to :a command) from ex input  
    and copies them to the given yank buffer.  
`:rx reg cmd`  
    Like :! command, executes cmd.  However, the contents of the  
    specified yank buffer is given to the command as input, and the  
    output of the command is written to the same buffer.  
`:rk reg path`  
    Connects to a unix socket, writes the contents of the given buffer  
    to it, and reads from the socket into the buffer.  
`:ra reg`  
    Similar to :@ command, executes the contents of the given buffer.  
    Before executing the buffer, however, it replaces ^rx with the  
    contents of buffer x, and ^vx with x, in which x is any character.  
`:ec[ho] msg`  
    Prints the given message (useful in ex scripts or q-commands).  

New key mappings:
- **^a** in normal mode: searches for the word under the cursor.
- **^p** in insert mode: inserts the contents of the default yank buffer.
- **^rX** in insert mode: inserts the contents of yank buffer X.
- **z>**, **z<**, **2z>**, and **2z<** in normal mode: changes the value of td option.
- **^e** and **^f** in insert mode: switches to the English and alternate keymap.
- **ze** and **zf** in normal mode: switches to the English and alternate keymap.
- **gu**, **gU**, and **g~** in normal mode: switches character case.
- **^l** in normal mode: updates terminal dimensions (after resizing it).
- **^]** and **^t** in normal mode: jumps to tag and pops tag stack.
- **gf** in normal mode: edits the file whose address is under the cursor.
- **gl** in normal mode: like gf, but it reads line and column numbers too.
- **^ws**, **^wo**, **^wc**, **^wj**, **^wk**, **^wx** in normal mode: manages windows.
- **^wgf**, **^wgl**, **^w^]** in normal mode: executes **gf**, **gl**, **^]** in a new window.
- **zj**, **zk**, **zD**: equivalent to _:b+_, _:b-_, _:b!_.
- **zJ**, **zK**: equivalent to _:next_, _:prev_.
- **q** in normal mode: see quick access section.
- **^a** in ex, search, and pipe prompts: inserts from history lines.

Other noteworthy differences with vi(1):
- Neatvi assumes POSIX extended regular expressions (ERE) in search
  patterns, conf.h variables, and even tags files.
- If paths start with _=_, they are assumed to be relative to the
  directory of the current file.
- Neatvi highlights files whose names end with ls as directory
  listings; the gl command edits the file under the cursor.  For
  instance, `git ls-files >ls && neatvi ls`.
- In addition to the standard single-letter yank buffers, Neatvi
  supports a set of extended buffers whose two-letter names begin
  with a backslash, like _\x_.
- If _EXINIT_ environment variable is defined as `so /path/to/exrc`,
  Neatvi executes the ex commands in this file at startup.

Note that in _:rs_ command, input lines are read from ex input stream
(unlike _:a_), to make it usable in _@_ commands and ex scripts (files
passed to _:so_).  This allows setting the value of yank buffers in ex
files, as the following example shows:

<pre>
  rs a
  :!git add %
  .
</pre>
OPTIONS
-------

To improve Neatvi's performance, shaping, character reordering, and
syntax highlighting can be disabled by defining the EXINIT environment
variable as `set noshape | set noorder | set nohl | set td=+2`.

Options supported in Neatvi:

- **td, textdirection**
  Current direction context.  The following values are meaningful:
  * +2: always left-to-right.
  * +1: follow conf.h's dircontexts[]; left-to-right for others.
  * -1: follow conf.h's dircontexts[]; right-to-left for others.
  * -2: always right-to-left.
  The default value is 0, which assumes left-to-right if the first
  character of the line is single-byte; otherwise, it follows
  dircontexts[].
- **shape**
  If set, Arabic/Farsi letter shaping will be performed.
- **order**
  If set, characters will be reordered based on the rules defined
  in conf.h.  If the value is 1, only lines with at least one
  multi-byte UTF-8 character are reordered.  If it is 2, all lines
  are reordered.
- **hl, highlight**
  If set, text will be highlighted based on syntax highlighting
  rules in conf.h.
- **hll, highlightline**
  If set, the current line will be highlighted.
- **lim, linelimit**
  Lines longer than this value are not reordered or highlighted.
- **ru, ruler**
  Indicates when to redraw the status line:
  * 0: never.
  * 1: always.
  * 2: when multiple windows are visible.
  * 4: when the current file changes.
- **hist, history**
  Indicates the number of lines remembered for ex, search, and
  pipe prompts.  Zero disables command history.
- **ai, autoindent**
  As in vi(1).
- **aw, autowrite**
  As in vi(1).
- **ic, ignorecase**
  As in vi(1).
- **wa, writeany**
  As in vi(1).

MARKS AND BUFFERS
-----------------

Special marks:
- \* the position of the previous change
- [ the first line of the previous change
- ] the last line of the previous change

Special yank buffers:
- / the previous search keyword
- : the previous ex command
- ! the previous pipe command
- % the name of the current file
- " the default yank buffer
- ; the current line
- . the last vi command

# cursor line number
- ^ cursor line offset
- \/ search history
- \: ex command history
- \! pipe command history

QUICK ACCESS
------------

When q is pressed in normal mode, Neatvi prints the list of buffers at
the bottom of the screen and waits for a key.  If the key is a digit,
it switches to its corresponding buffer.  If it is a letter and the
extended buffer with that letter is defined, the contents of that
buffer is executed.  Otherwise, Neatvi executes ECMD (defined in
conf.h) with the following parameters: i) the letter, ii) the current
file, iii) the current line number, and iv) the current line offset.

What ECMD writes to its standard output, Neatvi executes as ex
commands.  Q-commands can be used to add interesting features to
Neatvi, such as language-dependent IDE features, for instance by
connecting to an LSP (language server protocol) server.  ecmd.sh is an
example ECMD shell script.

AUTO-COMPLETION
---------------

When the hist option is nonzero, Neatvi suggests the most recent
matching entry when reading user input for :, /, and ! prompts; ^a
completes the input using the suggestion.

On the other hand, when in insert mode, ^a changes the value of the
buffer named ~ to the contents of the current line up to the cursor,
and executes the contents of \~ buffer (as :ra \~).  The buffer can
execute any ex command (for instance, :ec to print a message), and
change the contents of ~ to suggest a completion; a second ^a inserts
the suggestion.  The following lines define \~ to demonstrate how it
works.
<pre>
  rs \~
  ec completing '�~'
  rs ~
  completion
  �.. 
</pre>
In a real implementation, the \~ buffer can use external commands
(with rx or rk) to compute the completions.
