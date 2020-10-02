// This example assumes that you want to synchronize the store via other means
// than the Debugger interface. To do so, the application must track changes in
// data, and schedule these changes for synchronization.
//
// Basically, the application that wants to do synchronization must implement a
// memory (consistency) model. The stronger the model, the more guarantees the
// system gives the programmer about order of memory operations, but the
// trickier it is to implement.
//
// Sequential consistency, for example, is easy to
// understand: every memory operation appears to be executed in one single
// sequence, regardless which process in a distributed system executes the read
// or write. It is very expensive to implement such a model.
//
// The weakest possible model would give no guarantees; one would never know if
// a write to memory would be observable by others, and in which order.
// Designing hardware for such a model is easy...
//
// In my PhD thesis (Programming Models for Many-Core Architectures -- A
// Co-design Approach, 2014), I define Portable Memory Consistency, which
// strikes a balance between both. The hooks mentioned for PMC are available in
// libstored, implementation of them depends on the application. Following the
// thesis, any distributed memory model can be implemented on top of these
// hooks, including software cache coherency, and Distributed Shared Memory
// using only posted-writes.
//
// The store has two types of objects: variables and functions. Functions
// inherently have side effects. The hooks are not called for them, you should
// handle it by yourself if you need special synchronization. For variables,
// the hooks are in place.

int32 variable 1
int32=3 variable 2
(int32) function

