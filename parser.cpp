#include "parser.h"

#include "cc_ublox/message/CfgPrtUart.h"
#include "cc_ublox/message/NavPvt.h"
#include "comms/units.h"
#include "comms/process.h"

Parser::Parser(Napi::Env* env, Napi::Object* obj) {
    m_env = env;
	m_obj = obj;
};

Parser::~Parser() = default;

void Parser::parse(std::vector<uint8_t> bytes)
{
    processAllWithDispatch(&bytes[0], bytes.size(), m_frame, *this);
}

void Parser::handle(InNavPvt& msg)
{
    printf("UBX-NAV-PVT -> lon,lat: %f, %f\n", comms::units::getDegrees<double>(msg.field_lon()), comms::units::getDegrees<double>(msg.field_lat()));

	m_obj->Set(Napi::String::New(*m_env, "lon"), Napi::Number::New(*m_env, comms::units::getDegrees<double>(msg.field_lon())));
	m_obj->Set(Napi::String::New(*m_env, "lat"), Napi::Number::New(*m_env, comms::units::getDegrees<double>(msg.field_lat())));
}

void Parser::handle(InNavPosLlh& msg)
{
    printf("UBX-NAV-POSLLH -> lon,lat: %f, %f\n", comms::units::getDegrees<double>(msg.field_lon()), comms::units::getDegrees<double>(msg.field_lat()));
}

void Parser::handle(InMessage& msg)
{
    static_cast<void>(msg); // ignore
}

