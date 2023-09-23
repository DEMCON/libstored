#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -exuo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

if ! which brew > /dev/null && ! which port > /dev/null; then
	/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

do_brew=0
do_port=0

if which brew > /dev/null; then
	do_brew=1
elif which port > /dev/null; then
	do_port=1
else
	echo "No brew or port found."
	gotErr
fi

function do_install {
	if [[ ${do_brew} == 1 ]]; then
		for p in "$@"; do
			if ! brew ls --versions "$p" >/dev/null; then
				HOMEBREW_NO_AUTO_UPDATE=1 brew install "$p" || gotErr
			fi
		done
	elif [[ ${do_port} == 1 ]]; then
		sudo port -N install "$@" || gotErr
	fi
}

do_install cmake pkgconfig gnutls doxygen plantuml ninja coreutils git jq

if which python3 > /dev/null; then
	echo Skip install python3
elif [[ ${do_brew} == 1 ]]; then
	do_install python3
elif [[ ${do_port} == 1 ]]; then
	do_install python311
fi

python3 -m ensurepip

if [[ ${do_brew} == 1 ]]; then
	do_install zeromq
elif [[ ${do_port} == 1 ]]; then
	do_install zmq
fi

set +x

if [[ ${do_brew} == 1 ]]; then
	echo -e "\nSuggested packages to install manually:\n"
	echo -e "  brew install cppcheck clang-format@11 lcov"
elif [[ ${do_port} == 1 ]]; then
	echo -e "\nSuggested packages to install manually:\n"
	echo -e "  sudo port install cppcheck lcov"
fi
