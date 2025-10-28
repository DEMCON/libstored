# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

if(MINGW)
	function(install_dll name)
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${name}
			OUTPUT_VARIABLE dll_path
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(NOT "${dll_path}" STREQUAL "${name}")
			get_property(dlls GLOBAL PROPERTY LIBSTORED_RUNTIME_LIBS)
			set_property(GLOBAL PROPERTY LIBSTORED_RUNTIME_LIBS "${dlls};${dll_path}")
			install(FILES
				${dll_path}
				DESTINATION bin
			)
		endif()
	endfunction()

	install_dll(libgcc_s_seh-1.dll)
	install_dll(libstdc++-6.dll)
	install_dll(libwinpthread-1.dll)
endif()
