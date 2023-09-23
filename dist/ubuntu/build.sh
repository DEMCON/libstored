#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

cmake_opts=

if [[ `lsb_release -r -s | sed 's/\..*//'` -lt 20 ]]; then
	# Ubuntu 20.04 is required for PySide6. Exclude all components that
	# depend on PySide6.
	cmake_opts="${cmake_opts} -DLIBSTORED_PYLIBSTORED=OFF"
fi

. ../common/build.sh

popd > /dev/null
