#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function brew_install {
	if ! brew ls --versions "$1" >/dev/null; then
		HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
	fi
}

brew_install cmake
brew_install python3
brew_install pkgconfig
brew_install gnutls
brew_install doxygen
brew_install plantuml
brew_install ninja
brew_install coreutils
