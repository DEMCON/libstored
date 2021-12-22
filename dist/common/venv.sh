#!/bin/bash

set -euo pipefail

dir="$( cd "$(dirname "${BASH_SOURCE[0]}")"/..; pwd -P )"
venv_dir="${dir}/.venv"

in_venv=0
python3 "${dir}/common/check_venv.py" || in_venv=1

function venv_install {
	if [[ ! -e ${venv_dir} ]]; then
		if [[ ${in_venv} == 1 ]]; then
			echo "Preparing current venv with `which python3`..."
			python3 -m pip install --prefer-binary --upgrade wheel pip
			mkdir -p "${venv_dir}"
		else
			echo Installing venv in "${venv_dir}"...
			python3 -m venv "${venv_dir}"
			"${venv_dir}/bin/python3" -m pip install --prefer-binary --upgrade wheel pip
		fi
	fi
}

function venv_just_activate {
	venv_install

	if [[ ${in_venv} == 0 ]]; then
		set +u
		source "${venv_dir}/bin/activate"
		set -u
		if [ `readlink -f \`which python3\`` != `readlink -f ${venv_dir}/bin/python3` ]; then
			echo "activate failed" >&2
			return 1
		fi
	fi
}

function venv_requirements {
	venv_just_activate
	echo "Installing dependencies in "${venv_dir}" using `which python3`..."
	python3 -m pip install --prefer-binary --upgrade \
		-r "${dir}/common/requirements.txt"

	touch "${venv_dir}/.timestamp"
}

function venv_check {
	if [[	-e "${venv_dir}" &&
		"${dir}/common/requirements.txt" -ot "${venv_dir}/.timestamp"
	]]; then
		# Nothing to do
		return 0
	else
		venv_requirements
		return 1
	fi
}

function venv_activate {
	if venv_check; then
		venv_just_activate
	else
		true # Already activated
	fi
}

function venv_deactivate {
	if [[ `type -t deactivate` == "function" ]]; then
		set +u
		deactivate || true
		set -u
	fi
}

function venv_clean {
	venv_deactivate
	if [[ -e ${venv_dir} ]]; then
		rm -rf "${venv_dir}"
	fi
}

function venv_help {
	echo "Usage:"
	echo "    $0 install|check|clean"
	echo "    source $0 activate|deactivate"
	return 1
}

case "$#" in
	0) op="check";;
	1) op="$1";;
	*) venv_help;;
esac

case "${op}" in
	"minimal")
		venv_just_activate;;
	"install")
		venv_requirements;;
	"activate")
		venv_activate;;
	"deactivate")
		venv_deactivate;;
	"check")
		venv_check;;
	"clean")
		venv_clean;;
	*)
		venv_help;;
esac
