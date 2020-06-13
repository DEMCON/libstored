#include "Hello.h"

#include <cstdio>

static void list_cb(stored::Hello* h, char const* name, stored::Type::type type, void* buffer, size_t len) {
    printf("%s: type 0x%x, buffer %p, key %" PRIuPTR ", len %zu\n",
        name, (int)type, buffer, stored::Type::isFunction(type) ? 0 : h->bufferToKey(buffer), len);
}

int main()
{
    stored::Hello h;
    h.hello() = 42;
    h.world() = 3.14;

    printf("hello=%d world=%g\n", h.hello().as<int>(), h.world().get());
    printf("zero=%" PRId32 ", u64=%" PRIu64" f=%e\n", h.zero().get(), h.u64().get(), h.f().as<double>());

    h.find("/hello").variable<int32_t>() = 43;
    printf("hello=%d\n", h.hello().as<int>());

    h.list(&list_cb);

    return 0;
}

