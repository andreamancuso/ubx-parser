#pragma once

#include <napi.h>
#include <string>
#include <fstream>
#include <memory>
#include "index_db.h"
#include "shallow_parser.h"

class UbxLog;

class IndexWorker : public Napi::AsyncWorker {
public:
    IndexWorker(Napi::Env env,
                Napi::Promise::Deferred deferred,
                UbxLog* log,
                Napi::Object logRef,
                const std::string& path);

    void Execute() override;
    void OnOK() override;
    void OnError(const Napi::Error& error) override;

private:
    Napi::Promise::Deferred m_deferred;
    UbxLog* m_log;
    Napi::ObjectReference m_logRef;
    std::string m_path;
    std::unique_ptr<IndexDb> m_db;
    std::ifstream m_file;
};
