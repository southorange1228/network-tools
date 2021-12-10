#include "ping.cpp"


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Ping::Init(env, exports);
    return exports;
}

NODE_API_MODULE(hello, Init)
