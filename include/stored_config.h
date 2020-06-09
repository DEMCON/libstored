#ifndef __LIBSTORED_CONFIG_H
#  error Do not include this file directly, include <stored> instead.
#endif

#ifdef __cplusplus
namespace stored {
	struct Config : public DefaultConfig {
		// Override defaults from DefaultConfig here for your local setup.
		//static bool const EnableAssert = false;
	};
} // namespace
#endif // __cplusplus
