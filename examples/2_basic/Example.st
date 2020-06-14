// Did you enable syntax highlighting in Vim? See scripts/st.vim.

// The following types are supported:
bool       b
int8       i8
uint8      u8
int16      i16
uint16     u16
int32      i32
uint32     u32
int64      i64
uint64     u64
float      f
double     d

// A string is a UTF-8 string. Indicate the length of the string in bytes. This
// includes the \0 terminator.
string:4   s

// A blob is just a piece of memory. You can use it for any other buffer.
// Indicate the size of the blob in bytes.
blob:16    blob

// To override the initialized value, specify it like this:
int8=42    test42

// Initializer don't work on a string and blob, though.  Note that there should
// not be any whitespace around the =, because after from the first whitespace,
// the name is taken. So, this is not what you would expect, although the
// syntax highlighting might give you a hint:
int8 = 42  test42

// If you make this error a lot, you may prefer writing the name on the next
// line like this:
int8=42
another 42

// The name of a variable consists of ASCII 0x20 (space) till 0x7e (~).  Spaces
// at the start and end of the named are stripped off, and consecutive spaces
// inside the name are merged. Otherwise, the name is as you type.
bool=true So, this is a perfectly fine name, even if it includes characters like ~!@#$%; and so on.

// In the C++ interface, all invalid characters are mapped and merged to _.
// Check the generated Example.h!

// Every type can be an array by adding [size]:
int8[4] four ints
int16[3]=2 three ints // All initialized to two.
string:4[2] two strings

// The generated store does not have arrays, however. The generator will expand
// the array by adding [n] to the name. So, this is identical as the arrays
// above:
//int8 four ints[0]
//int8 four ints[1]
//int8 four ints[2]
//int8 four ints[3]
//int16=2 three ints[0]
//int16=2 three ints[1]
//int16=2 three ints[2]
//string:4 two strings[0]
//string:4 two strings[1]

