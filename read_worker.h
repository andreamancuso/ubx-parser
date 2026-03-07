#pragma once

#include <napi.h>
#include <string>
#include <vector>
#include <cstdint>

class UbxLog;

class ReadWorker : public Napi::AsyncWorker {
public:
    ReadWorker(Napi::Env env,
               Napi::Promise::Deferred deferred,
               UbxLog* log,
               Napi::Object logRef,
               const std::string& path,
               int64_t offset,
               int32_t length);

    void Execute() override;
    void OnOK() override;
    void OnError(const Napi::Error& error) override;

private:
    Napi::Promise::Deferred m_deferred;
    UbxLog* m_log;
    Napi::ObjectReference m_logRef;
    std::string m_path;
    int64_t m_offset;
    int32_t m_length;
    std::vector<uint8_t> m_buf;
};
