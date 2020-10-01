#include "ExampleSync.h"

#include <cstring>

namespace stored {

/*!
 * \brief Non-zero initialized variables in the store.
 * \details This block of memory is copied to the start of the buffer upon initialization.
 */
static unsigned char const ExampleSyncData_bufferinit[4] = {
0x03, 0x00, 0x00, 0x00, 
};

/*!
 * \brief Constructor.
 */
ExampleSyncData::ExampleSyncData()
    : buffer()
{
    memcpy(buffer, ExampleSyncData_bufferinit, sizeof(ExampleSyncData_bufferinit));
}

/*!
 * \brief Directory listing with full names in binary directory format.
 * \details This is typically used for listing the directory.
 * \see \ref libstored_directory for the format
 */
static uint8_t const ExampleSyncData_directory_full[65] = {
0x2f, 0x76, 0x24, 0x00, 0x61, 0x00, 0x00, 0x72, 0x00, 0x00, 0x69, 0x00, 0x00, 0x61, 0x00, 0x00, 
0x62, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x65, 0x00, 0x00, 0x20, 0x00, 0x00, 0x32, 0x04, 0x00, 0xbb, 
0x00, 0x31, 0x00, 0x00, 0xbb, 0x08, 0x66, 0x00, 0x00, 0x75, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x63, 
0x00, 0x00, 0x74, 0x00, 0x00, 0x69, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x6e, 0x00, 0x00, 0xfb, 0x01, 
0x00, 
};

/*!
 * \brief Directory listing with short (but unambiguous) names in binary directory format.
 * \detials This is typically used for searching the directory.
 * \see \ref libstored_directory for the format
 */
static uint8_t const ExampleSyncData_directory[21] = {
0x2f, 0x76, 0x0d, 0x00, 0x08, 0x32, 0x04, 0x00, 0xbb, 0x00, 0x31, 0x00, 0x00, 0xbb, 0x08, 0x66, 
0x00, 0x00, 0xfb, 0x01, 0x00, 
};

/*!
 * \brief Retuns the long directory.
 * \details When not available, the short directory is returned.
 */
uint8_t const* ExampleSyncData::longDirectory() {
    return Config::FullNames ? (uint8_t const*)ExampleSyncData_directory_full : (uint8_t const*)ExampleSyncData_directory;
}

/*!
 * \brief Returns the short directory.
 */
uint8_t const* ExampleSyncData::shortDirectory() {
    return (uint8_t const*)ExampleSyncData_directory;
}

} // namespace stored
