#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"
pushd "${here}" > /dev/null

cmake_opts="-DCMAKE_TOOLCHAIN_FILE:PATH='${here}/toolchain-mingw.cmake'"
do_test_run=0
. ../common/build.sh

popd > /dev/null
