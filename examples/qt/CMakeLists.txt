# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

if(TARGET Qt5::Core)
	find_package(Qt5 COMPONENTS Gui Quick)
	set(QtNs Qt5)

	if(CMAKE_CXX_STANDARD GREATER_EQUAL 11 AND CMAKE_CXX_STANDARD LESS 98)
		set(Qt_FOUND ${Qt5_FOUND})
	endif()
else()
	find_package(Qt6 COMPONENTS Gui Quick)
	set(QtNs Qt)

	if(CMAKE_CXX_STANDARD GREATER_EQUAL 17 AND CMAKE_CXX_STANDARD LESS 98)
		set(Qt_FOUND ${Qt6_FOUND})
	endif()
endif()

if(Qt_FOUND)
	set(CMAKE_AUTORCC ON)

	add_executable(qt main.cpp main.qrc main.qml)

	libstored_generate(TARGET qt STORES ExampleQtStore.st QT)

	target_link_libraries(qt PUBLIC ${QtNs}::Gui ${QtNs}::Quick)

	install(TARGETS qt RUNTIME DESTINATION bin)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_definitions(qt PUBLIC QT_QML_DEBUG)
	endif()

	add_launch_json(qt)
endif()
