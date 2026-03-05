#include "parser.h"

struct MessageSchemaGen {
    Napi::Env& env;
    Napi::Array& result;
    uint32_t idx = 0;

    template <typename TMsg>
    void operator()() {
        TMsg msg;
        Napi::Object entry = Napi::Object::New(env);
        entry.Set("name", Napi::String::New(env, msg.doName()));
        Napi::Object fields = Napi::Object::New(env);
        SchemaFieldVisitor visitor{env, fields};
        comms::util::tupleForEach(msg.fields(), visitor);
        entry.Set("fields", fields);
        result.Set(idx++, entry);
    }
};

Napi::Array Parser::schema(Napi::Env& env) {
    Napi::Array result = Napi::Array::New(env);
    MessageSchemaGen gen{env, result};
    comms::util::tupleForEachType<AllInMessages>(gen);
    return result;
}
