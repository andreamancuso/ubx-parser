#pragma once

#include <vector>
#include <napi.h>
#include "cc_ublox/Message.h"
#include "cc_ublox/input/AllMessages.h"
#include "cc_ublox/frame/UbloxFrame.h"

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

    template <typename TMsg>
    void handle(TMsg& msg);

    void handle(InMessage& msg);

    Napi::Array parse(const std::vector<uint8_t>& bytes);

    static Napi::Array schema(Napi::Env& env);

private:
    Frame m_frame;
    Napi::Env* m_env;
    std::vector<Napi::Object> m_messages;
};
