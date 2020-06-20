#!/bin/bash

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

case `uname -s` in
	Linux*)
		sudo apt install -y build-essential git-core gcc-multilib cmake python3 python3-pip gdb-multiarch clang-tidy doxygen
		pip3 install jinja2 textx pyzmq;;
	Darwin*)
		brew install cmake python3
		pip3 install jinja2 textx pyzmq;;
	*)
		echo "Unknown OS"
		exit 1;;
esac

