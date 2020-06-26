if(LIBSTORED_HAVE_LIBZMQ)
	# If Zth includes libzmq or there already exists a libzmq target, skip including our external.
	if(NOT ZTH_HAVE_LIBZMQ AND NOT libzmq)
		set(BUILD_TESTS OFF CACHE BOOL "Disabling building tests")
		set(BUILD_STATIC OFF CACHE BOOL "Don't build static library" FORCE)
		add_subdirectory(libzmq)
	endif()
endif()
