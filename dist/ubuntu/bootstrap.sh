#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -exuo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

sudo apt install -y \
	build-essential git-core cmake pkg-config \
	python3 python3-pip python3-setuptools \
	doxygen plantuml python3-venv python3-dev

[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch

set +x

echo -e "\nSuggested packages to install manually:\n"
echo -e "  sudo apt install -y ninja-build spin gdb-multiarch cppcheck \\"
echo -e "                      clang clang-tidy clang-format-11 libzmq3-dev \\"
echo -e "                      qt6-base-dev qt6-declarative-dev afl++\n"

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	echo -e "\nQt6 (and therefore PySide6) requires Ubuntu 20.04 or later."
	echo -e "You seem to have an older version (`lsb_release -r -s`)."
	echo -e "Python packages will not be available in this build."
fi
