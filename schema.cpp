#include "schema.h"
#include "ubx_common.h"
#include "schema_visitor.h"
#include "cc_ublox/Message.h"
#include "cc_ublox/input/AllMessages.h"
#include "comms/util/Tuple.h"

// Lightweight message type — no Handler, no dispatch overhead
using SchemaMessage = cc_ublox::Message<
    comms::option::ReadIterator<const std::uint8_t*>
>;
using AllSchemaMessages = cc_ublox::input::AllMessages<SchemaMessage>;

struct MessageSchemaGen {
    Napi::Env& env;
    Napi::Array& result;
    uint32_t idx = 0;

    template <typename TMsg>
    void operator()() {
        TMsg msg;
        Napi::Object entry = Napi::Object::New(env);
        setNameAndVariant(env, entry, msg.doName());
        Napi::Object fields = Napi::Object::New(env);
        SchemaFieldVisitor visitor{env, fields};
        comms::util::tupleForEach(msg.fields(), visitor);
        entry.Set("fields", fields);
        result.Set(idx++, entry);
    }
};

Napi::Array ubxSchema(Napi::Env& env) {
    Napi::Array result = Napi::Array::New(env);
    MessageSchemaGen gen{env, result};
    comms::util::tupleForEachType<AllSchemaMessages>(gen);
    return result;
}
