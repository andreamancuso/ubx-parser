#pragma once

#include <napi.h>
#include <fstream>
#include <memory>
#include <cstdint>
#include "index_db.h"

class UbxLog : public Napi::ObjectWrap<UbxLog> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    UbxLog(const Napi::CallbackInfo& info);
    ~UbxLog();

    // Called by IndexWorker on completion to transfer ownership
    void setReady(std::unique_ptr<IndexDb> db, std::ifstream&& file);

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

    // Deep-parse: read frame bytes from file and decode via Parser
    Napi::Value deepParseAt(Napi::Env env, const MessageEntry& entry);

    void ensureOpen(Napi::Env env);

    std::unique_ptr<IndexDb> m_db;
    std::ifstream m_file;
    int64_t m_cursor = -1; // before first message
    bool m_ready = false;
    bool m_closed = false;

    static Napi::FunctionReference s_constructor;
};
