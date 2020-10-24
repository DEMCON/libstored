// Any store definition is compatible with the FPGA, but there are a few
// details to mention.

// For a C++ target, /t would be a function, which is read-only.  Functions
// are, however, not supported on FPGA, and will be silently ignored from the
// store VHDL file. Moreover, there is no Embedded Debugger that can be
// connected to the FPGA; it has only the synchronizer interface. A read-only
// register simply just updates it data very clock cycle.
uint32 t (clk)
(int32) ignored function

uint32 default register
uint32=101 initialized register
uint16 read-only register

uint32 default register write count

// Any data type is supported, but the AXI interface only gives access
// to variables of at most 32-bit. All others are available in the
// direct interface, but not via AXI.
bool bool
string:6 string
blob:3 blob

