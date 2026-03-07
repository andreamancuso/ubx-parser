#pragma once

#include <string>
#include <napi.h>

inline void setNameAndVariant(Napi::Env& env, Napi::Object& obj, const char* fullName) {
    std::string full(fullName);
    auto pos = full.rfind(" (");
    if (pos != std::string::npos && full.back() == ')') {
        obj.Set("name", Napi::String::New(env, full.substr(0, pos)));
        obj.Set("variant", Napi::String::New(env, full.substr(pos + 2, full.size() - pos - 3)));
    } else {
        obj.Set("name", Napi::String::New(env, full));
    }
}
