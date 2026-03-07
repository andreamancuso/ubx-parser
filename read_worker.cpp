#include "read_worker.h"
#include "ubx_log.h"
#include <fstream>

ReadWorker::ReadWorker(Napi::Env env,
                       Napi::Promise::Deferred deferred,
                       UbxLog* log,
                       Napi::Object logRef,
                       const std::string& path,
                       int64_t offset,
                       int32_t length)
    : Napi::AsyncWorker(env)
    , m_deferred(deferred)
    , m_log(log)
    , m_path(path)
    , m_offset(offset)
    , m_length(length)
{
    m_logRef = Napi::Persistent(logRef);
}

void ReadWorker::Execute() {
    std::ifstream file(m_path, std::ios::binary);
    if (!file.is_open()) {
        SetError("Failed to open file: " + m_path);
        return;
    }

    file.seekg(m_offset);
    if (!file.good()) {
        SetError("Failed to seek in file");
        return;
    }

    m_buf.resize(static_cast<std::size_t>(m_length));
    file.read(reinterpret_cast<char*>(m_buf.data()), m_length);
    if (!file.good()) {
        SetError("Failed to read from file");
        return;
    }
}

void ReadWorker::OnOK() {
    Napi::Env env = Env();
    Napi::Value result = m_log->deepParse(env, m_buf);
    m_deferred.Resolve(result);
}

void ReadWorker::OnError(const Napi::Error& error) {
    m_deferred.Reject(error.Value());
}
