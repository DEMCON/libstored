#!/bin/bash

set -e

pushd "$( cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P )" > /dev/null

function nproc {
	sysctl -n hw.logicalcpu
}

cmake_opts=
. ../common/build.sh

popd > /dev/null
