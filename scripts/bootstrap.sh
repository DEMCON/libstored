#!/bin/bash

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

case `uname -s` in
	Linux*)
		sudo apt update
		sudo apt install -y build-essential valgrind git-core gcc-multilib cmake \
			gdb-multiarch cppcheck doxygen pkg-config \
			python3 python3-pip python3-setuptools \
			python3-pyqt5 python3-pyqt5.qtquick \
			spin plantuml
		python3 -m pip install wheel
		python3 -m pip install -r scripts/requirements.txt
		;;
	Darwin*)
		function install_or_upgrade {
			if brew ls --versions "$1" >/dev/null; then
				HOMEBREW_NO_AUTO_UPDATE=1 brew upgrade "$1"
			else
				HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
			fi
		}
		install_or_upgrade cmake
		install_or_upgrade python3
		install_or_upgrade pyqt
		install_or_upgrade pkgconfig
		install_or_upgrade gnutls
		install_or_upgrade doxygen
		install_or_upgrade plantuml
		pip3 install wheel
		pip3 install -r scripts/requirements.txt
		;;
	*)
		echo "Unknown OS"
		exit 1;;
esac

popd > /dev/null
