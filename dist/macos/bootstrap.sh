#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
brew_install git
brew_install zeromq
brew_install jq
