#include "parser.h"
#include "ubx_common.h"
#include "comms/process.h"
#include "comms/util/Tuple.h"
#include "field_visitor.h"

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
    std::size_t consumed = 0;
    if (!bytes.empty()) {
        consumed = comms::processAllWithDispatch(&bytes[0], bytes.size(), m_frame, *this);
    }

    Napi::Array result = Napi::Array::New(*m_env, m_messages.size());
    for (std::size_t i = 0; i < m_messages.size(); ++i) {
        result.Set(static_cast<uint32_t>(i), m_messages[i]);
    }
    result.Set("consumed", Napi::Number::New(*m_env, static_cast<double>(consumed)));
    return result;
}

Napi::Value Parser::parseOne(Napi::Env& env, const std::vector<uint8_t>& bytes) {
    m_env = &env;
    m_messages.clear();

    if (!bytes.empty()) {
        comms::processAllWithDispatch(&bytes[0], bytes.size(), m_frame, *this);
    }

    if (!m_messages.empty()) {
        return m_messages[0];
    }
    return env.Null();
}
