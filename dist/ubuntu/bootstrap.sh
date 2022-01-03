#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

sudo apt install -y \
	build-essential git-core cmake pkg-config \
	python3 python3-pip python3-setuptools \
	doxygen plantuml

[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch

echo -e "\nSuggested packages to install manually:\n"
echo -e "  sudo apt install -y ninja-build spin gdb-multiarch cppcheck \\"
echo -e "                      clang clang-tidy clang-format libzmq3-dev\n"

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	echo -e "\nQt6 (and therefore PySide6) requires Ubuntu 20.04 or later."
	echo -e "You seem to have an older version (`lsb_release -r -s`)."
	echo -e "Python packages will not be available in this build."
fi
