# BayOS
---

## astyle
<details>
<summary>We use a custom astyle to format our code:</summary>

```
--mode=c
--style=java
--indent=tab=4
--align-pointer=name
--pad-oper
--pad-comma
--pad-header
--unpad-paren
--add-braces
--keep-one-line-blocks
--lineend=linux
--suffix=none
```
</details>

## Hooks

<details>
<summary>We use a custom pre-commit hook.</summary>
Basically the example hook with an implementation for astyle.

```
#!/bin/sh
#
# An example hook script to verify what is about to be committed.
# Called by "git commit" with no arguments.  The hook should
# exit with non-zero status after issuing an appropriate message if
# it wants to stop the commit.
#
# To enable this hook, rename this file to "pre-commit".

if git rev-parse --verify HEAD >/dev/null 2>&1
then
	against=HEAD
else
	# Initial commit: diff against an empty tree object
	against=$(git hash-object -t tree /dev/null)
fi

# If you want to allow non-ASCII filenames set this variable to true.
allownonascii=$(git config --type=bool hooks.allownonascii)

# Redirect output to stderr.
exec 1>&2

# Cross platform projects tend to avoid non-ASCII filenames; prevent
# them from being added to the repository. We exploit the fact that the
# printable range starts at the space character and ends with tilde.
if [ "$allownonascii" != "true" ] &&
	# Note that the use of brackets around a tr range is ok here, (it's
	# even required, for portability to Solaris 10's /usr/bin/tr), since
	# the square bracket bytes happen to fall in the designated range.
	test $(git diff-index --cached --name-only --diff-filter=A -z $against |
	 LC_ALL=C tr -d '[ -~]\0' | wc -c) != 0
then
	cat <<\EOF
Error: Attempt to add a non-ASCII file name.

This can cause problems if you want to work with people on other platforms.

To be portable it is advisable to rename the file.

If you know what you are doing you can disable this check using:

  git config hooks.allownonascii true
EOF
	exit 1
fi
# --- astyle ---------------------------------------------------------
files=$(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(c|h)$')

if [ -n "$files" ]; then
	echo "astyle: formatiere gestagte C-Dateien..."
	for f in $files; do
		astyle --options=.astylerc "$f"
		git add "$f"
	done
	echo "astyle: fertig."
fi
# --------------------------------------------------------------------
# If there are whitespace errors, print the offending file names and fail.
exec git diff-index --check --cached $against --
```
</details>
