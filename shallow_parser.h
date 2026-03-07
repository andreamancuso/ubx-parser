#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

struct FrameInfo {
    std::string name;       // e.g. "NAV-PVT"
    int64_t offset;         // byte offset in file
    int32_t length;         // total frame length (sync + header + payload + checksum)
};

class ShallowParser {
public:
    // Feed a chunk of data. fileOffset is the offset within the file where this
    // chunk starts (before prepending any leftover from previous calls).
    // Calls onFrame for each valid frame found.
    void feed(const uint8_t* data, std::size_t size, int64_t fileOffset,
              const std::function<void(const FrameInfo&)>& onFrame);

    // Returns any leftover bytes that couldn't form a complete frame.
    // The caller must track the file offset adjustment.
    std::size_t leftoverSize() const { return m_leftover.size(); }

private:
    std::vector<uint8_t> m_leftover;

    // Resolve class+id to message name
    static const char* resolveName(uint16_t msgId);
};
