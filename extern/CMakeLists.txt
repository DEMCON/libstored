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

		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			add_compile_options(-Wno-unused-parameter -Wno-inconsistent-missing-override -Wno-unused-const-variable -Wno-missing-field-initializers -Wno-unused-variable)
		endif()

		add_subdirectory(libzmq)
	endif()
endif()

if(LIBSTORED_HAVE_HEATSHRINK)
	add_library(heatshrink STATIC heatshrink/heatshrink_encoder.c heatshrink/heatshrink_decoder.c)
	if(MSVC)
		target_compile_options(heatshrink PRIVATE /W1)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			target_compile_options(heatshrink PUBLIC /MTd)
		else()
			target_compile_options(heatshrink PUBLIC /MT)
		endif()
	endif()
	target_include_directories(heatshrink PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/heatshrink)
	target_compile_definitions(heatshrink INTERFACE STORED_HAVE_HEATSHRINK)
endif()

