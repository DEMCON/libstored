# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

if(MINGW AND CMAKE_CROSSCOMPILING)
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
		OUTPUT_VARIABLE mingw_version
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	set(dlls
		/usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libgcc_s_seh-1.dll
		/usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libstdc++-6.dll
		/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
	)
	get_property(LIBSTORED_RUNTIME_LIBS GLOBAL PROPERTY LIBSTORED_RUNTIME_LIBS)
	set_property(GLOBAL PROPERTY LIBSTORED_RUNTIME_LIBS "${LIBSTORED_RUNTIME_LIBS};${dlls}")

	install(FILES
		${dlls}
		DESTINATION bin
	)
endif()
