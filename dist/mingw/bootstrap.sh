#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"

sudo apt install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

CC=x86_64-w64-mingw32-gcc
CXX=x86_64-w64-mingw32-g++
"${here}/../ubuntu/bootstrap.sh" || exit 1
