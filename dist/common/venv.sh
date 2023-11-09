#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -uo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 2
}

function venv_help {
	echo "Usage:"
	echo "    $0 [-h] [-p <python_exe>] [install|check|clean]"
	echo "    source $0 activate|deactivate"
	echo ""
	echo "where"
	echo "  -h   Show this help"
	echo "  -p   Use given python executable. Default: ${python}"
	return 2
}

python=`which python3`

if which greadlink > /dev/null; then
	# For macOS, with coreutils
	readlink=greadlink
else
	readlink=readlink
fi

while getopts "hp:" arg; do
	case "${arg}" in
		h)
			venv_help
			exit 0
			;;
		p)
			python=`which "${OPTARG}"`
			if [[ -z ${python} ]]; then
				echo "Cannot find python executable '${OPTARG}'" >&2
				exit 2
			fi
			;;
		*)
			venv_help
			exit 2
	esac
done
shift $((OPTIND-1))

if [[ ! -x ${python} ]]; then
	echo "Cannot find python executable '${python}'" >&2
	exit 2
fi

dir="$( cd "$(dirname "${BASH_SOURCE[0]}")"/..; pwd -P )"
venv_dir="${dir}/venv"

in_venv=0
"${python}" "${dir}/common/check_venv.py" || in_venv=1

function venv_install {
	if [[ ! -e ${venv_dir} ]]; then
		pkg=wheel

		if [[ ${VENV_SKIP_PIP:-0} != 1 ]]; then
			pkg="${pkg} pip"
		fi

		if [[ ${in_venv} == 1 ]]; then
			echo "Preparing current venv with ${python}..."
			"${python}" -m pip install --upgrade ${pkg} || gotErr
			mkdir -p "${venv_dir}" || gotErr
		else
			echo Installing venv in "${venv_dir}"...
			"${python}" -m venv "${venv_dir}" || gotErr
			"${venv_dir}/bin/python3" -m pip install --upgrade ${pkg} || gotErr
		fi
	fi
}

function venv_just_activate {
	venv_install

	if [[ ${in_venv} == 0 ]]; then
		set +u
		source "${venv_dir}/bin/activate"
		set -u

		if [[ `${readlink} -f \`which python3\`` != `${readlink} -f ${venv_dir}/bin/python3` ]]; then
			echo "activate failed" >&2
			return 2
		fi

		# Use venv's python3 from now on.
		python=`which python3`
	fi
}

function venv_requirements_minimal {
	venv_just_activate
	echo "Installing minimal dependencies in "${venv_dir}" using ${python}..."
	"${python}" -m pip install --prefer-binary --upgrade \
		-r "${dir}/common/requirements-minimal.txt" || gotErr

	touch "${venv_dir}/.timestamp-minimal" || gotErr
}

function venv_requirements {
	venv_just_activate
	echo "Installing dependencies in "${venv_dir}" using ${python}..."
	"${python}" -m pip install --prefer-binary --upgrade \
		-r "${dir}/common/requirements.txt" || gotErr

	touch "${venv_dir}/.timestamp-minimal" || gotErr
	touch "${venv_dir}/.timestamp" || gotErr
}

function venv_check_minimal {
	if [[	-e "${venv_dir}" &&
		"${dir}/common/requirements-minimal.txt" -ot "${venv_dir}/.timestamp-minimal"
	]]; then
		# Nothing to do
		return 0
	else
		venv_requirements_minimal
		return 1
	fi
}

function venv_check {
	if [[	-e "${venv_dir}" &&
		"${dir}/common/requirements-minimal.txt" -ot "${venv_dir}/.timestamp-minimal" &&
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
		rm -rf "${venv_dir}" || gotErr
	fi
}

case "$#" in
	0)
		op="check";;
	1)
		op="$1";;
	*)
		venv_help
		exit 2
		;;
esac

case "${op}" in
	"minimal")
		venv_just_activate;;
	"install")
		venv_requirements;;
	"install-minimal")
		venv_requirements_minimal;;
	"activate")
		venv_activate;;
	"deactivate")
		venv_deactivate;;
	"check")
		venv_check;;
	"check-minimal")
		venv_check_minimal;;
	"clean")
		venv_clean;;
	*)
		venv_help;;
esac
