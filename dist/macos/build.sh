#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -e

function nproc {
	sysctl -n hw.logicalcpu
}

if ! pkg-config libzmq > /dev/null; then
	# Brew does not seem to add libzmq.pc to the search path.
	libzmq_version="`brew info --json=v1 zmq | jq ".[].installed[].version" -r`"
	libzmq_path="/usr/local/Cellar/zeromq/${libzmq_version}/lib/pkgconfig"
	if [[ -e ${libzmq_path}/libzmq.pc ]]; then
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${libzmq_path}"
	fi
fi

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

cmake_opts=
. ../common/build.sh

popd > /dev/null
