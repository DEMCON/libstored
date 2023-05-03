# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

if(MINGW)
	function(install_dll name)
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${name}
			OUTPUT_VARIABLE dll_path
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		install(FILES
			${dll_path}
			DESTINATION bin
		)
	endfunction()

	install_dll(libgcc_s_seh-1.dll)
	install_dll(libstdc++-6.dll)
	install_dll(libwinpthread-1.dll)
endif()
