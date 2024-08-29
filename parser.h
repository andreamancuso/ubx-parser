#include <vector>

#include "cc_ublox/Message.h"
#include "cc_ublox/message/NavPvt.h"
#include "cc_ublox/message/NavPosllh.h"
#include "cc_ublox/frame/UbloxFrame.h"

class Parser
{
    using InMessage =
        cc_ublox::Message<
            comms::option::ReadIterator<const std::uint8_t*>,
            comms::option::Handler<Parser> // Dispatch to this object
        >;

    using OutBuffer = std::vector<std::uint8_t>;
    using OutMessage =
        cc_ublox::Message<
            comms::option::IdInfoInterface,
            comms::option::WriteIterator<std::back_insert_iterator<OutBuffer>>,
            comms::option::LengthInfoInterface
        >;

    using InNavPvt = cc_ublox::message::NavPvt<InMessage>;
    using InNavPosLlh = cc_ublox::message::NavPosllh<InMessage>;

public:
    Parser();
    ~Parser();

    void parse(std::vector<uint8_t> bytes);

    void handle(InNavPvt& msg);

    void handle(InNavPosLlh& msg);

    void handle(InMessage& msg);

private:

    using AllInMessages =
        std::tuple<

    InNavPosLlh,InNavPvt
        >;

    using Frame = cc_ublox::frame::UbloxFrame<InMessage, AllInMessages>;

    Frame m_frame;
};
