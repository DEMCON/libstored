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

config Debug dev test
config Debug nodev test
config Release nodev nozmq test
config Release test
config Debug nodev C++03 test
config Debug nodev C++11 test
config Debug nodev C++14 test
config Debug nodev C++17 test
config Release nodev C++03 test
config Release nodev C++11 test
config Release nodev C++14 test
config Release nodev C++17 test
config Debug nodev noheatshrink test
config Release noheatshrink test
config Debug nodev noexamples test
config Release nodev noexamples test
config Debug nodev san zth test
config Release nodev zth test
config Debug nodev test -- -DLIBSTORED_DISABLE_EXCEPTIONS=ON -DLIBSTORED_DISABLE_RTTI=ON

popd > /dev/null
