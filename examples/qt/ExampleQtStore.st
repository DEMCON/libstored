// Normal variables will become Q_PROPERTYs, with appropriate ...Changed
// signals.
int16=1 hello
double=2.3 world
string:5 to everyone

// The Qt wrapper supports all types, except blob, ptr32 and ptr64.  So, these
// are hidden.
blob:3 blob
ptr32 ptr

// For functions, slots are generated. They lack the ...Changed signal.
(bool) bool function
(string:6) string function
