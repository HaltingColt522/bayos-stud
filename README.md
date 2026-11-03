How to build with xbstrap:

install xbstrap with pip (pipx on arch):
- pipx install xbstrap
- pipx ensurepath

install dependencies: (meson, ninja, make, nasm, gcc and more)
- ...

clone this repo:
- git clone https://github.com/haltingcolt522/bayos-stud

create build dir:
- mkdir -pv build && cd build

init xbstrap:
- xbstrap init ../bayos-stud

build and run with:
- xbstrap run qemu

to rebuild something: (examples)
- xbstrap install --rebuild kernel
- xbstrap install --rebuild mlibc

to reconfigure something (after meson.build was changed)
- xbstrap install --reconfigure kernel
