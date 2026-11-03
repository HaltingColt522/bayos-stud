#!/bin/sh
set -e
CDPATH=
unset CDPATH GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR GIT_INDEX_FILE GIT_OBJECT_DIRECTORY

dest="$1"
mkdir -p "$dest"

get() {  # url dir commit
    d="$dest/$2"
    if [ -d "$d/.git" ]; then
        # Already at the pinned commit: nothing to do.
        if [ "$(git -C "$d" rev-parse HEAD)" = "$3" ]; then
            return
        fi
        git -C "$d" fetch -q origin
    else
        rm -rf "$d"
        git clone -q "$1" "$d"
    fi
    git -C "$d" checkout -q "$3"
}

get https://github.com/osdev0/freestanding-c-hdrs.git        freestanding-c-hdrs 38fed4e1e3365733ddbfa03b0a28936243ad31e9
get https://github.com/osdev0/cc-runtime.git                 cc-runtime          9891629daf3703d0ddecd295772df7da82ab25b9
get https://github.com/Limine-Bootloader/limine-protocol.git limine-protocol     3a0526b700e356f0eac1b71a77697b3fd1c707a3
