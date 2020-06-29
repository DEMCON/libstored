#!/bin/bash

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

case `uname -s` in
	Linux*)
		sudo apt install -y build-essential git-core gcc-multilib cmake \
			gdb-multiarch clang-tidy doxygen \
			python3 python3-pip \
			python3-pyqt5 python3-pyqt5.qtquick
		/usr/bin/pip3 install jinja2 textx pyzmq pyside2 pyserial lognplot

		if ! python3 -V | awk '$2~/^3.[0-6]/{exit 1}'; then
			# lognplot's server needs python 3.7+
			pyver=3.7
			sudo apt install -y python$pyver
			python$pyver -m pip install pip
			pip$pyver install PyQt5 lognplot
		fi
		;;
	Darwin*)
		brew install cmake python3 pyqt
		pip3 install jinja2 textx pyzmq pyside2 pyserial lognplot
		;;
	*)
		echo "Unknown OS"
		exit 1;;
esac

