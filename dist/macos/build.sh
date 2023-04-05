#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -e

function nproc {
	sysctl -n hw.logicalcpu
}

if which brew > /dev/null && which pkg-config > /dev/null && ! pkg-config libzmq > /dev/null; then
	# Brew does not seem to add libzmq.pc to the search path.
	libzmq_version="`brew info --json=v1 zmq | jq ".[].installed[].version" -r`"
	libzmq_path="/usr/local/Cellar/zeromq/${libzmq_version}/lib/pkgconfig"
	if [[ -e ${libzmq_path}/libzmq.pc ]]; then
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${libzmq_path}"
	fi
fi

cmake_opts=

ver=`sw_vers -productVersion`
ver_major=`echo ${ver} | sed -E 's/^([0-9]+)\.([0-9]+).*/\1/'`
ver_minor=`echo ${ver} | sed -E 's/^([0-9]+)\.([0-9]+).*/\2/'`
if [[ ${ver_major} < 10 ]] || [[ ${ver_major} -eq 10 ]] && [[ ${ver_minor} < 14 ]]; then
	# macos 10.14 is required for PySide6. Exclude all components that
	# depend on PySide6.
	cmake_opts="${cmake_opts} -DLIBSTORED_PYLIBSTORED=OFF"
fi

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

. ../common/build.sh

popd > /dev/null
