#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -e

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

cmake_opts=

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	# Ubuntu 20.04 is required for PySide6. Exclude all components that
	# depend on PySide6.
	cmake_opts="${cmake_opts} -DLIBSTORED_PYLIBSTORED=OFF"
fi

. ../common/build.sh

popd > /dev/null
