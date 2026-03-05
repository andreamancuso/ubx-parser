#include "parser.h"
#include "comms/process.h"
#include "comms/util/Tuple.h"
#include "field_visitor.h"
#include "schema_visitor.h"

static void setNameAndVariant(Napi::Env& env, Napi::Object& obj, const char* fullName) {
    std::string full(fullName);
    auto pos = full.rfind(" (");
    if (pos != std::string::npos && full.back() == ')') {
        obj.Set("name", Napi::String::New(env, full.substr(0, pos)));
        obj.Set("variant", Napi::String::New(env, full.substr(pos + 2, full.size() - pos - 3)));
    } else {
        obj.Set("name", Napi::String::New(env, full));
    }
}

// Generic handler — dispatched for all concrete message types
template <typename TMsg>
void Parser::handle(TMsg& msg) {
    Napi::Object msgObj = Napi::Object::New(*m_env);
    setNameAndVariant(*m_env, msgObj, msg.doName());

    FieldToJs visitor{*m_env, msgObj};
    comms::util::tupleForEach(msg.fields(), visitor);

    m_messages.push_back(msgObj);
}

// Catch-all for unrecognized messages
void Parser::handle(InMessage& msg) {
    static_cast<void>(msg);
}

Napi::Array Parser::parse(const std::vector<uint8_t>& bytes) {
    if (!bytes.empty()) {
        comms::processAllWithDispatch(&bytes[0], bytes.size(), m_frame, *this);
    }

    Napi::Array result = Napi::Array::New(*m_env, m_messages.size());
    for (std::size_t i = 0; i < m_messages.size(); ++i) {
        result.Set(static_cast<uint32_t>(i), m_messages[i]);
    }
    return result;
}

// --- Schema generation ---

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

Napi::Array Parser::schema(Napi::Env& env) {
    Napi::Array result = Napi::Array::New(env);
    MessageSchemaGen gen{env, result};
    comms::util::tupleForEachType<AllInMessages>(gen);
    return result;
}
