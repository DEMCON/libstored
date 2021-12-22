#!/bin/bash

# This file is sourced from a dist/*/ directory. Do not call directly.

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function show_help {
	echo "Usage: $0 [CMAKE_BUILD_TYPE [<other cmake arguments>]]"
	exit 2
}

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &> /dev/null; pwd -P)"
dist_dir="$(pwd -P)"

case ${1:-} in
	-\?|-h|--help)
		show_help;;
esac

if which ninja > /dev/null; then
	cmake_opts="${cmake_opts} -G Ninja"
fi

if [[ ! -z "${1:-}" ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_BUILD_TYPE=${1}"
	shift || true
fi

if [[ ! -z ${CC:-} ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_C_COMPILER='${CC}'"
fi

if [[ ! -z ${CXX:-} ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_CXX_COMPILER='${CXX}'"
fi

if [[ -e ${dist_dir}/CMakeLists.txt ]]; then
	cmake_opts="${cmake_opts} -DLIBSTORED_DIST_DIR='${dist_dir}'"
fi

mkdir -p build
pushd build > /dev/null

cmake -DCMAKE_MODULE_PATH="${repo}/dist/common" \
	-DCMAKE_INSTALL_PREFIX="${dist_dir}/build/deploy" \
	${cmake_opts} "$@" ../../..

cmake --build . -j`nproc`
cmake --build . --target install -j`nproc`

popd > /dev/null
