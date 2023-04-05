#!/bin/bash

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
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

if ! which brew > /dev/null && ! which port > /dev/null; then
	/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

function do_install {
	if which brew > /dev/null; then
		for p in "$@"; do
			if ! brew ls --versions "$p" >/dev/null; then
				HOMEBREW_NO_AUTO_UPDATE=1 brew install "$p" || gotErr
			fi
		done
	elif which port > /dev/null; then
		sudo port -N install "$@" || gotErr
	else
		echo "No brew or port found."
		gotErr
	fi
}

do_install cmake pkgconfig gnutls doxygen plantuml ninja coreutils git jq

if which brew > /dev/null; then
	do_install zeromq
elif which port > /dev/null; then
	do_install zmq
fi

if which python3 > /dev/null; then
	echo Skip install python3
elif which brew > /dev/null; then
	do_install python3
elif which port > /dev/null; then
	do_install python310
	sudo port -N select --set python3 python310
fi

sudo python3 -m ensurepip
