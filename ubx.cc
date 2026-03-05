#include <napi.h>
#include "parser.h"

static Napi::Value Parse(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::Error::New(info.Env(), "Expected exactly one argument")
        .ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  if (!info[0].IsTypedArray()) {
    Napi::Error::New(info.Env(), "Expected a TypedArray")
        .ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  Napi::TypedArray typedArray = info[0].As<Napi::TypedArray>();

  if (typedArray.TypedArrayType() != napi_uint8_array) {
    Napi::Error::New(info.Env(), "Expected a Uint8Array")
        .ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  Napi::Uint8Array uint8Array = typedArray.As<Napi::Uint8Array>();

  std::vector<uint8_t> bytes(uint8Array.Data(),
                             uint8Array.Data() + uint8Array.ElementLength());

  Napi::Env env = info.Env();
  Parser p(&env);
  return p.parse(bytes);
}

static Napi::Value Schema(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Parser::schema(env);
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports["parse"] = Napi::Function::New(env, Parse);
  exports["schema"] = Napi::Function::New(env, Schema);
  return exports;
}

NODE_API_MODULE(ubx_parser, Init)
