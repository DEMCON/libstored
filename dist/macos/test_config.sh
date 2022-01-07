#!/bin/bash

# Run this script to build various configurations.

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

function config {
	echo -e "\n\n============================"
	echo -e "== Running config: $*\n"
	[[ ! -e build ]] || rm -rf build
	./build.sh "$@"
}

# By default, this uses clang.
config Debug dev test
config Debug nodev test
config Release test
config Debug nodev C++03 test
config Debug nodev C++17 test
config Release nodev C++03 test
config Release nodev C++17 test

if which g++-11 > /dev/null; then
	# This is gcc
	config Debug dev test CXX=g++-11 CC=gcc-11
	config Debug nodev test CXX=g++-11 CC=gcc-11
	config Release test CXX=g++-11 CC=gcc-11
	config Debug nodev zth test CXX=g++-11 CC=gcc-11
	config Release zth test CXX=g++-11 CC=gcc-11
fi

popd > /dev/null
