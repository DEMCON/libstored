#!/bin/bash

# Usage: build.sh [CMAKE_BUILD_TYPE [<other cmake arguments>]]


function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

function numproc {
	case `uname -s` in
		Linux*)
			nproc;;
		Darwin*)
			sysctl -n hw.logicalcpu;;
		*)
			echo 1;;
	esac
}

trap gotErr ERR

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

git submodule update --init --recursive

if [ -e build ]; then
	if [ ! -z "$1" ]; then
		# Override build type
		pushd build > /dev/null
		BUILD_TYPE="$1"
		shift || true
		cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. "$@"
		popd > /dev/null
	fi
else
	if [ -z "$1" ]; then
		BUILD_TYPE=Debug
	else
		BUILD_TYPE="$1"
	fi

	mkdir build
	pushd build > /dev/null
	shift || true
	cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. "$@"
	popd > /dev/null
fi

pushd build > /dev/null
cmake --build . -- -j`numproc` all
popd > /dev/null

