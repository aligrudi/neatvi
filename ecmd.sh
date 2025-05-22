#!/bin/sh
# This is an example ECMD script for Neatvi (see conf.h).
#
# When q is pressed in normal mode, Neatvi prints the list of buffers
# and waits for a key.  If the key is a digit, it switches to its
# corresponding buffer.  If it is a letter and the extended buffer
# with that letter is defined, the contents of that buffer is executed.
# Otherwise, Neatvi executes ECMD with the following parameters:
# i) the letter, ii) the current file, iii) the current line number,
# and iv) the current line offset.
#
# This files demonstrates how to implement such q-commands.

# git add %
ecmd_a() {
	# We can also invoke it directly here: git add $1
	echo '!git add %'
}

# Open ./ls as directory listing.
ecmd_l() {
	# We define \l here; ql executes \l register if it is
	# defined, so we do not have the overhead of executing ECMD
	# the second time.
	echo 'rs \l'
	echo 'e ls'
	echo '.'
	# Now we can ask Neatvi to execute \l.
	echo '@\l'
}

# Make qq equivalent to <control>-^.
ecmd_q() {
	# Again we define \q here, while we could have simply
	# written: echo "e #"
	echo 'rs \q'
	echo 'e #'
	echo '.'
	echo '@\q'
}

# Generate and view stag-generated file outline; use gl command on its lines.
ecmd_o() {
	if stag -a "$path" >/tmp/.tags.ls 2>/dev/null; then
		echo "e +1 /tmp/.tags.ls | :e"
	fi
}

# Open an email in a neatmail listing file.
ecmd_m() {
	path="$1"
	lnum="$2"
	loc=$(sed -E -n "${lnum}s/^[A-Z]+([0-9]+(@[^ \t]+)?).*$/\\1/p" <$path)
	echo "ec $loc"
	if test -n "$loc"; then
		if neatmail pg -s -b path/to/mbox -i $loc >.cur.mail 2>/dev/null; then
			echo "e +1 .cur.mail | e"
		else
			echo "ec neatmail failed"
		fi
	fi
}

# Goto definition for Go; uses gopls (not very efficient without using LSP).
ecmd_d() {
	loc=$(gopls definition $1:$2:$3)
	if test -n "$loc"; then
		echo $loc | sed -E 's/^([^:]+):([^:]+):([^:]+).*$/:e +\2 \1/'
	else
		echo "ec gopls failed"
	fi
}

# Find references for Go.  Use gl command on each line.
ecmd_f() {
	if gopls references $1:$2:$3 >.list.ls; then
		echo "e +1 .list.ls | :e"
	else
		echo "ec gopls failed"
	fi
}

ecmd_"$@" 2>/dev/null || echo "ec unknown command"
