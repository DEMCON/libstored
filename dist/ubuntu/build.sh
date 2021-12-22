#!/bin/bash

set -e

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

[[ ! -z ${CC:-} ]]  || CC=gcc
[[ ! -z ${CXX:-} ]] || CXX=g++

cmake_opts=

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	# Ubuntu 20.04 is required for PySide6. Exclude all components that
	# depend on PySide6.
	cmake_opts="${cmake_opts} -DLIBSTORED_PYLIBSTORED=OFF"
fi

. ../common/build.sh

popd > /dev/null
