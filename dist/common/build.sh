#!/bin/bash

# This file is sourced from a dist/*/ directory. Do not call directly.

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function show_help {
	echo -e "Usage: $0 [<opt>...] [--] [<other cmake arguments>]\n"
	echo "where opt is:"
	echo "  Debug RelWithDebInfo Release"
	echo "        Set CMAKE_BUILD_TYPE to this value"
	echo "  C++98 C++03 C++11 C++14 C++17 C++20"
	echo "        Set the C++ standard"
	echo "  dev   Enable development-related options"
	echo "  test  Enable building and running tests"
	echo "  zmq   Enable ZeroMQ integration"
	echo "  nozmq Disable ZeroMQ integration"
	exit 2
}

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &> /dev/null; pwd -P)"
dist_dir="$(pwd -P)"

build_type=
do_test=
do_test_run=${do_test_run:-1}
use_ninja=1
support_test=1
par=-j`nproc`

while [[ ! -z ${1:-} ]]; do
	case "${1}" in
		"-?"|-h|--help)
			show_help;;
		Debug)
			build_type=Debug;;
		RelWithDebInfo)
			build_type=RelWithDebInfo;;
		Release)
			build_type=Release;;
		C++98|C++03)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=98 -DCMAKE_C_STANDARD=99"
			support_test=0
			;;
		C++11)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=11 -DCMAKE_C_STANDARD=11"
			support_test=0
			;;
		C++14)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=14 -DCMAKE_C_STANDARD=11";;
		C++17)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_STANDARD=11";;
		C++20)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=20 -DCMAKE_C_STANDARD=11";;
		dev)
			cmake_opts="${cmake_opts} -DLIBSTORED_DEV=ON"
			do_test=1
			;;
		nodev)
			cmake_opts="${cmake_opts} -DLIBSTORED_DEV=OFF";;
		test)
			do_test=1;;
		notest)
			do_test=0;;
		notestrun)
			do_test_run=0;;
		examples)
			cmake_opts="${cmake_opts} -DLIBSTORED_EXAMPLES=ON";;
		noexamples)
			cmake_opts="${cmake_opts} -DLIBSTORED_EXAMPLES=OFF";;
		zmq)
			cmake_opts="${cmake_opts} -DLIBSTORED_HAVE_LIBZMQ=ON";;
		nozmq)
			cmake_opts="${cmake_opts} -DLIBSTORED_HAVE_LIBZMQ=OFF";;
		heatshrink)
			cmake_opts="${cmake_opts} -DLIBSTORED_HAVE_HEATSHRINK=ON";;
		noheatshrink)
			cmake_opts="${cmake_opts} -DLIBSTORED_HAVE_HEATSHRINK=OFF";;
		san)
			cmake_opts="${cmake_opts} -DLIBSTORED_ENABLE_ASAN=ON -DLIBSTORED_ENABLE_LSAN=ON -DLIBSTORED_ENABLE_UBSAN=ON";;
		nosan)
			cmake_opts="${cmake_opts} -DLIBSTORED_ENABLE_ASAN=OFF -DLIBSTORED_ENABLE_LSAN=OFF -DLIBSTORED_ENABLE_UBSAN=OFF";;
		CC=*)
			CC="${1#CC=}";;
		CXX=*)
			CXX="${1#CXX=}";;
		make)
			use_ninja=0;;
		--)
			# Stop parsing
			shift
			break
			;;
		-j*)
			par="${1}";;
		-*|*)
			# Only cmake flags follow.
			break;;
	esac
	shift
done

if [[ ${use_ninja} == 1 ]]; then
	if which ninja > /dev/null; then
		cmake_opts="${cmake_opts} -G Ninja"
	fi
fi

if [[ ! -z "${build_type}" ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_BUILD_TYPE=${build_type}"
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

if [[ ${support_test} == 0 ]]; then
	do_test=
fi

if [[ ! -z ${do_test} ]]; then
	if [[ ${do_test} == 1 ]]; then
		cmake_opts="${cmake_opts} -DLIBSTORED_TESTS=ON"
	else
		cmake_opts="${cmake_opts} -DLIBSTORED_TESTS=OFF"
	fi
fi

mkdir -p build
pushd build > /dev/null

cmake -DCMAKE_MODULE_PATH="${repo}/dist/common" \
	-DCMAKE_INSTALL_PREFIX="${dist_dir}/build/deploy" \
	${cmake_opts} "$@" ../../..

cmake --build . "${par}"
cmake --build . --target install "${par}"

if [[ ${do_test} == 1 ]] && [[ ${do_test_run} == 1 ]]; then
	CTEST_OUTPUT_ON_FAILURE=1 cmake --build . --target test
fi

popd > /dev/null
