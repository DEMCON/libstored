#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

function nproc {
	sysctl -n hw.logicalcpu
}

if which brew > /dev/null && which pkg-config > /dev/null && ! pkg-config libzmq > /dev/null; then
	# Brew does not seem to add libzmq.pc to the search path.
	libzmq_version="`brew info --json=v1 zmq | jq ".[].installed[].version" -r`"
	libzmq_path="/usr/local/Cellar/zeromq/${libzmq_version}/lib/pkgconfig"
	if [[ -e ${libzmq_path}/libzmq.pc ]]; then
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:+${PKG_CONFIG_PATH}:}${libzmq_path}"
	fi
fi

# By default, clang compiles for C++98, but it seems to crash on some headers lately.
# Default to C++14.
cmake_opts="-DCMAKE_CXX_STANDARD=14 -DCMAKE_C_STANDARD=11"

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

. ../common/build.sh

popd > /dev/null
