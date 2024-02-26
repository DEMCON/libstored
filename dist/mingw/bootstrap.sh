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

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"

if [[ ${UID:-1} -eq 0 ]] && [[ ! which sudo > /dev/null ]]; then
	# Running as root in some docker image?
	apt install -y sudo || ( apt update && apt install -y sudo )
fi

sudo apt install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

CC=x86_64-w64-mingw32-gcc
CXX=x86_64-w64-mingw32-g++
"${here}/../ubuntu/bootstrap.sh" || exit 1
