// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Application-specific stored::DefaultConfig override.
 *
 * Copy this file to your project, and make sure the include path is such that
 * your file is found before the default supplied one.
 */

#ifndef LIBSTORED_CONFIG_H
#	error Do not include this file directly, include <stored> instead.
#endif

#ifndef STORED_CONFIG_H
#	define STORED_CONFIG_H

#	ifdef __cplusplus
namespace stored {
/*!
 * \brief Example of a configuration that override the stored::DefaultConfig.
 * \see stored_config.h
 */
struct Config : public DefaultConfig {
	// Override defaults from DefaultConfig here for your local setup.
	// static bool const HookSetOnChangeOnly = true;

	// template <typename T>
	// struct Allocator {
	//	typedef MyAllocator<T> type;
	// };

	// ...
};
} // namespace stored
#	endif // __cplusplus
#endif	       // STORED_CONFIG_H
