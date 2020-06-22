#!/bin/bash

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

if [ -z "$1" ]; then
	BUILD_TYPE=Debug
else
	BUILD_TYPE=$1
fi

git submodule init
git submodule update

mkdir -p build
pushd build > /dev/null
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
cmake --build . -- -j`nproc` all
popd > /dev/null

