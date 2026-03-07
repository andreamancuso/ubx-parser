#include "shallow_parser.h"
#include "cc_ublox/MsgId.h"
#include "cc_ublox/field/MsgIdCommon.h"
#include <cstdio>

void ShallowParser::feed(const uint8_t* data, std::size_t size, int64_t fileOffset,
                          const std::function<void(const FrameInfo&)>& onFrame)
{
    // Prepend any leftover from previous call
    std::vector<uint8_t> buf;
    int64_t baseOffset;

    if (!m_leftover.empty()) {
        baseOffset = fileOffset - static_cast<int64_t>(m_leftover.size());
        buf.reserve(m_leftover.size() + size);
        buf.insert(buf.end(), m_leftover.begin(), m_leftover.end());
        buf.insert(buf.end(), data, data + size);
        m_leftover.clear();
    } else {
        baseOffset = fileOffset;
        // Avoid copy — work directly on data pointer
        buf.assign(data, data + size);
    }

    const uint8_t* ptr = buf.data();
    std::size_t remaining = buf.size();
    std::size_t pos = 0;

    while (pos + 8 <= remaining) { // minimum frame: 2 sync + 2 id + 2 len + 0 payload + 2 ck
        // Scan for sync bytes
        if (ptr[pos] != 0xB5 || ptr[pos + 1] != 0x62) {
            ++pos;
            continue;
        }

        // Read class + ID
        uint8_t cls = ptr[pos + 2];
        uint8_t id = ptr[pos + 3];

        // Read payload length (little-endian)
        uint16_t payloadLen = static_cast<uint16_t>(ptr[pos + 4]) |
                              (static_cast<uint16_t>(ptr[pos + 5]) << 8);

        // Sanity check: no real UBX message exceeds 8KB payload
        constexpr uint16_t MAX_PAYLOAD_LEN = 8192;
        if (payloadLen > MAX_PAYLOAD_LEN) {
            pos += 2;
            continue;
        }

        // Total frame length: 2 sync + 1 class + 1 id + 2 length + payload + 2 checksum
        int32_t frameLen = 6 + static_cast<int32_t>(payloadLen) + 2;

        // Check if we have the complete frame
        if (pos + static_cast<std::size_t>(frameLen) > remaining) {
            break; // incomplete frame — save as leftover
        }

        // Validate checksum (Fletcher-8 over class + id + length + payload)
        uint8_t ckA = 0, ckB = 0;
        for (std::size_t i = pos + 2; i < pos + 6 + payloadLen; ++i) {
            ckA += ptr[i];
            ckB += ckA;
        }

        uint8_t expectedCkA = ptr[pos + 6 + payloadLen];
        uint8_t expectedCkB = ptr[pos + 6 + payloadLen + 1];

        if (ckA != expectedCkA || ckB != expectedCkB) {
            // Bad checksum — skip this sync and keep scanning
            pos += 2;
            continue;
        }

        // Valid frame — resolve name
        uint16_t msgId = (static_cast<uint16_t>(cls) << 8) | id;
        const char* name = resolveName(msgId);

        FrameInfo info;
        if (name) {
            info.name = name;
        } else {
            char hexBuf[12];
            std::snprintf(hexBuf, sizeof(hexBuf), "0x%04X", msgId);
            info.name = hexBuf;
        }
        info.offset = baseOffset + static_cast<int64_t>(pos);
        info.length = frameLen;

        onFrame(info);

        pos += static_cast<std::size_t>(frameLen);
    }

    // Save leftover
    if (pos < remaining) {
        m_leftover.assign(ptr + pos, ptr + remaining);
    }
}

const char* ShallowParser::resolveName(uint16_t msgId) {
    return cc_ublox::field::MsgIdCommon::valueName(
        static_cast<cc_ublox::MsgId>(msgId));
}
