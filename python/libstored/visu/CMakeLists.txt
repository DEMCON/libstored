# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

find_program(RCC_EXE pyside6-rcc PATHS $ENV{HOME}/.local/bin REQUIRED)

cmake_policy(SET CMP0058 NEW)

function(libstored_visu target rcc)
	foreach(f IN LISTS ARGN)
		get_filename_component(f_abs ${f} ABSOLUTE)

		if(f_abs MATCHES "^(.*/)?main\\.qml$")
			set(qrc_main "${f_abs}")
			string(REGEX REPLACE "^(.*/)?main.qml$" "\\1" qrc_prefix ${f_abs})
		endif()
	endforeach()

	if(NOT qrc_main)
		message(FATAL_ERROR "Missing main.qml input for ${target}")
	endif()

	string(LENGTH "${qrc_prefix}" qrc_prefix_len)

	set(qrc "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n<qresource>\n")
	foreach(f IN LISTS ARGN)
		get_filename_component(f_abs ${f} ABSOLUTE)
		if(qrc_prefix_len GREATER 0)
			string(SUBSTRING "${f_abs}" 0 ${qrc_prefix_len} f_prefix)
			if(f_prefix STREQUAL qrc_prefix)
				string(SUBSTRING "${f_abs}" ${qrc_prefix_len} -1 f_alias)
				set(qrc "${qrc}<file alias=\"${f_alias}\">${f_abs}</file>\n")
			else()
				set(qrc "${qrc}<file>${f_abs}</file>\n")
			endif()
		else()
			set(qrc "${qrc}<file>${f_abs}</file>\n")
		endif()
	endforeach()
	set(qrc "${qrc}</qresource>\n</RCC>\n")

	get_filename_component(rcc ${rcc} ABSOLUTE)
	file(GENERATE OUTPUT ${rcc}.qrc CONTENT "${qrc}")

	add_custom_command(
		OUTPUT ${rcc}
		DEPENDS
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Input.qml
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Measurement.qml
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/StoreObject.qml
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/qmldir
			${ARGN}
			${rcc}.qrc
		COMMAND ${RCC_EXE} $<SHELL_PATH:${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc> $<SHELL_PATH:${rcc}.qrc> -o $<SHELL_PATH:${rcc}>
		COMMENT "Generating ${target} visu"
		VERBATIM
	)

	set_property(SOURCE ${rcc}.qrc ${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
		PROPERTY AUTORCC OFF)
	add_custom_target(${target} DEPENDS ${rcc} SOURCES
		${rcc}.qrc
		${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc)
endfunction()

if(LIBSTORED_EXAMPLES)
	libstored_visu(visu-example ${CMAKE_CURRENT_SOURCE_DIR}/visu_example.py example/main.qml)
endif()

