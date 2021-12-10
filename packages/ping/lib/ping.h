#include "napi.h"

using namespace std;

struct PingOptions {
    string addr;
    int timeout;
    int retry;
};

class Ping: public Napi::ObjectWrap<Ping> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Ping(const Napi::CallbackInfo &info);

private:
    Napi::Value start(const Napi::CallbackInfo &info);
    Napi::Value end(const Napi::CallbackInfo &info);

    PingOptions options;
};