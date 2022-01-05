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

config Debug dev
config Debug nodev
config Release nodev nozmq
config Release
config Release nodev zth

popd > /dev/null
