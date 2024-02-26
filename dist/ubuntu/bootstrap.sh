#!/bin/bash

# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -exuo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

if [[ ${UID:-0} -eq 0 ]] && ! which sudo > /dev/null; then
	# Running as root in some docker image?
	apt install -y sudo || ( apt update && apt install -y sudo )
fi

sudo apt install -y \
	build-essential git-core cmake pkg-config \
	python3 python3-pip python3-setuptools \
	doxygen plantuml python3-venv python3-dev lsb-release \
	libgl1 libegl1 libxkbcommon0

[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch

set +x

echo -e "\nSuggested packages to install manually:\n"
echo -e "  sudo apt install -y ninja-build spin gdb-multiarch cppcheck flawfinder\\"
echo -e "                      clang clang-tidy clang-format-11 libzmq3-dev \\"
echo -e "                      qt6-base-dev qt6-declarative-dev afl++ lcov\n"

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	echo -e "\nQt6 (and therefore PySide6) requires Ubuntu 20.04 or later."
	echo -e "You seem to have an older version (`lsb_release -r -s`)."
	echo -e "Python packages will not be available in this build."
fi
