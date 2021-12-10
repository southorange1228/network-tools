#include "ping.h"

using namespace std;

Napi::Object Ping::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env,"Ping",{
        InstanceMethod("start",&Ping::start),
        InstanceMethod("end",&Ping::end)
    });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("Ping", func);
    return exports;
}

Ping::Ping(const Napi::CallbackInfo &info): Napi::ObjectWrap<Ping>(info) {
    Napi::Env env = info.Env();

    int length = info.Length();

    if (length <= 0) {
        Napi::TypeError::New(env, "Option should provide").ThrowAsJavaScriptException();
        return;
    }
    if (!info[0].IsObject()) {
        Napi::TypeError::New(env,"Option should be object").ThrowAsJavaScriptException();
        return;
    }
    Napi::Object obj = info[0].ToObject();
    if (obj.Has("addr") && obj.Get("addr").IsString()){
        this->options.addr = obj.Get("addr").As<Napi::String>();
    }else{
        Napi::TypeError::New(env,"addr should be string").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("retry") && obj.Get("retry").IsNumber()){
        this->options.retry = obj.Get("retry").As<Napi::Number>();
    }else{
        Napi::TypeError::New(env,"retry should be number").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("timeout")){
        if (obj.Get("timeout").IsNumber()){
            this->options.timeout = obj.Get("timeout").As<Napi::Number>();
        }else{
            Napi::TypeError::New(env,"retry should be number").ThrowAsJavaScriptException();
            return;
        }
    }
}

Napi::Value Ping::start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env,100);
}

Napi::Value Ping::end(const Napi::CallbackInfo &info)  {
    Napi::Env env = info.Env();
    return Napi::String::New(env,"hello world");
}