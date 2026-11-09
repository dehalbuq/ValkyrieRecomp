/* Independently authored C++ dynamic-local-initializer fixture only. */
static int call_count;
__attribute__((noinline)) static int make_value() {
    ++call_count;
    return 410+call_count;
}
extern "C" int phase41_fixture_marker(void){return 0x41424341;}
extern "C" int phase41_cpp_static(void) {
    static int only_once=make_value();
    return only_once;
}
extern "C" int phase41_cpp_calls(void){return call_count;}
