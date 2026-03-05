#pragma once

#include <vector>
#include <napi.h>
#include "cc_ublox/Message.h"
#include "cc_ublox/input/AllMessages.h"
#include "cc_ublox/frame/UbloxFrame.h"
#include "comms/process.h"
#include "comms/util/Tuple.h"
#include "field_visitor.h"
#include "schema_visitor.h"

class Parser
{
    using InMessage =
        cc_ublox::Message<
            comms::option::ReadIterator<const std::uint8_t*>,
            comms::option::Handler<Parser>
        >;

    using AllInMessages = cc_ublox::input::AllMessages<InMessage>;
    using Frame = cc_ublox::frame::UbloxFrame<InMessage, AllInMessages>;

public:
    Parser(Napi::Env* env) : m_env(env) {}

    // Generic handler — dispatched for all concrete message types
    template <typename TMsg>
    void handle(TMsg& msg) {
        Napi::Object msgObj = Napi::Object::New(*m_env);
        msgObj.Set("name", Napi::String::New(*m_env, msg.doName()));

        FieldToJs visitor{*m_env, msgObj};
        comms::util::tupleForEach(msg.fields(), visitor);

        m_messages.push_back(msgObj);
    }

    // Catch-all for unrecognized messages
    void handle(InMessage& msg) {
        static_cast<void>(msg);
    }

    Napi::Array parse(const std::vector<uint8_t>& bytes) {
        if (!bytes.empty()) {
            comms::processAllWithDispatch(&bytes[0], bytes.size(), m_frame, *this);
        }

        Napi::Array result = Napi::Array::New(*m_env, m_messages.size());
        for (std::size_t i = 0; i < m_messages.size(); ++i) {
            result.Set(static_cast<uint32_t>(i), m_messages[i]);
        }
        return result;
    }

    static Napi::Array schema(Napi::Env& env);

private:
    Frame m_frame;
    Napi::Env* m_env;
    std::vector<Napi::Object> m_messages;
};
