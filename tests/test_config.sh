#!/bin/bash

# Run this script (without arguments) to build and test libstored
# using different sets of configuration options.

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

function config {
	scripts/build.sh "$@" && make -C build test
}

config Debug -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=ON -DCMAKE_CXX_STANDARD=14
config Debug -DLIBSTORED_HAVE_LIBZMQ=OFF -DLIBSTORED_HAVE_HEATSHRINK=OFF -DCMAKE_CXX_STANDARD=11
config Debug -DLIBSTORED_HAVE_LIBZMQ=OFF -DLIBSTORED_HAVE_HEATSHRINK=ON -DLIBSTORED_POLL=POLL -DCMAKE_CXX_STANDARD=11
config Debug -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=OFF -DLIBSTORED_POLL=ZMQ -DCMAKE_CXX_STANDARD=11
config Debug -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=ON -DLIBSTORED_POLL=LOOP -DCMAKE_CXX_STANDARD=14

config Release -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=ON -DCMAKE_CXX_STANDARD=14
config Release -DLIBSTORED_HAVE_LIBZMQ=OFF -DLIBSTORED_HAVE_HEATSHRINK=OFF -DCMAKE_CXX_STANDARD=11
config Release -DLIBSTORED_HAVE_LIBZMQ=OFF -DLIBSTORED_HAVE_HEATSHRINK=ON -DLIBSTORED_POLL=POLL -DCMAKE_CXX_STANDARD=11
config Release -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=OFF -DLIBSTORED_POLL=ZMQ -DCMAKE_CXX_STANDARD=11
config Release -DLIBSTORED_HAVE_LIBZMQ=ON -DLIBSTORED_HAVE_HEATSHRINK=ON -DLIBSTORED_POLL=LOOP -DCMAKE_CXX_STANDARD=14

popd > /dev/null
