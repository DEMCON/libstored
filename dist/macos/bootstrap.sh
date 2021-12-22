#!/bin/bash

set -euo pipfail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function install_or_upgrade {
	if brew ls --versions "$1" >/dev/null; then
		HOMEBREW_NO_AUTO_UPDATE=1 brew upgrade "$1"
	else
		HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
	fi
}

install_or_upgrade cmake
install_or_upgrade python3
install_or_upgrade pkgconfig
install_or_upgrade gnutls
install_or_upgrade doxygen
install_or_upgrade plantuml
install_or_upgrade ninja
