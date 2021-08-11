if(LIBSTORED_HAVE_LIBZMQ)
	# If Zth includes libzmq or there already exists a libzmq target, skip including our external.
	if(NOT ZTH_HAVE_LIBZMQ AND NOT libzmq)
		if(MINGW)
			# See https://github.com/zeromq/libzmq/issues/3859
			set(ZMQ_CV_IMPL "win32api" CACHE STRING "Override auto-detect" FORCE)
		endif()

		if(APPLE)
			# Make sure home-brew headers are in there.
			include_directories(BEFORE SYSTEM /usr/local/include)
		endif()

		set(BUILD_TESTS OFF CACHE BOOL "Disabling building tests")
		set(BUILD_STATIC OFF CACHE BOOL "Don't build static library" FORCE)

		if(LIBSTORED_ENABLE_ASAN)
			set(ENABLE_ASAN ON)
		endif()

		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			add_compile_options(-Wno-unused-parameter -Wno-inconsistent-missing-override -Wno-unused-const-variable -Wno-missing-field-initializers -Wno-unused-variable)
		endif()

		set(CMAKE_CXX_STANDARD_prev ${CMAKE_CXX_STANDARD})
		if(CMAKE_CXX_STANDARD EQUAL 98)
			set(CMAKE_CXX_STANDARD 11)
		endif()
		add_subdirectory(libzmq)
		set(CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD_prev})
	endif()
endif()


