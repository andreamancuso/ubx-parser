#pragma once

#include <napi.h>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include "index_db.h"

class Parser;

class UbxLog : public Napi::ObjectWrap<UbxLog> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    UbxLog(const Napi::CallbackInfo& info);
    ~UbxLog();

    // Called by IndexWorker on completion to transfer ownership
    void setReady(std::unique_ptr<IndexDb> db, const std::string& path);

    // Deep-parse a buffer of frame bytes into a JS message object (called by ReadWorker::OnOK)
    Napi::Value deepParse(Napi::Env env, const std::vector<uint8_t>& buf);

private:
    // Static factory: UbxLog.open(path) -> Promise<UbxLog>
    static Napi::Value Open(const Napi::CallbackInfo& info);

    // Instance methods
    Napi::Value Count(const Napi::CallbackInfo& info);
    Napi::Value MessageTypes(const Napi::CallbackInfo& info);
    Napi::Value Next(const Napi::CallbackInfo& info);
    Napi::Value Prev(const Napi::CallbackInfo& info);
    Napi::Value Seek(const Napi::CallbackInfo& info);
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value Close(const Napi::CallbackInfo& info);

    Napi::Value queueRead(Napi::Env env, const MessageEntry& entry);

    bool ensureOpen(Napi::Env env);

    std::unique_ptr<IndexDb> m_db;
    std::unique_ptr<Parser> m_parser;
    std::string m_path;
    int64_t m_cursor = -1; // before first message
    bool m_ready = false;
    bool m_closed = false;

    static Napi::FunctionReference s_constructor;
};
