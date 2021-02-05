find_program(RCC_EXE pyside2-rcc PATHS $ENV{HOME}/.local/bin REQUIRED)

function(libstored_visu target rcc)
	foreach(f IN LISTS ARGN)
		if(f MATCHES "^(.*/)?main\\.qml$")
			set(qrc_main "${f}")
			string(REGEX REPLACE "^(.*/)?main.qml$" "\\1" qrc_prefix ${f})
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
			string(SUBSTRING "${f}" 0 ${qrc_prefix_len} f_prefix)
			if(f_prefix STREQUAL qrc_prefix)
				string(SUBSTRING "${f}" ${qrc_prefix_len} -1 f_alias)
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
			${LIBSTORED_SOURCE_DIR}/client/ed2/visu/visu.qrc
			${LIBSTORED_SOURCE_DIR}/client/ed2/visu/Components/Input.qml
			${LIBSTORED_SOURCE_DIR}/client/ed2/visu/Components/Measurement.qml
			${LIBSTORED_SOURCE_DIR}/client/ed2/visu/Components/StoreObject.qml
			${LIBSTORED_SOURCE_DIR}/client/ed2/visu/Components/qmldir
			${ARGN}
			${rcc}.qrc
		COMMAND ${RCC_EXE} $<SHELL_PATH:${LIBSTORED_SOURCE_DIR}/client/ed2/visu/visu.qrc> $<SHELL_PATH:${rcc}.qrc> -o $<SHELL_PATH:${rcc}>
		COMMENT "Generating ${target} visu"
		VERBATIM
	)

	add_custom_target(${target} DEPENDS ${rcc})
endfunction()

libstored_visu(visu-example ${CMAKE_CURRENT_SOURCE_DIR}/visu_example.py example/main.qml)

