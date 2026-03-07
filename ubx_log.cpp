#include "ubx_log.h"
#include "index_worker.h"
#include "parser.h"

Napi::FunctionReference UbxLog::s_constructor;

Napi::Object UbxLog::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "UbxLog", {
        StaticMethod<&UbxLog::Open>("open"),
        InstanceMethod<&UbxLog::Count>("count"),
        InstanceMethod<&UbxLog::MessageTypes>("messageTypes"),
        InstanceMethod<&UbxLog::Next>("next"),
        InstanceMethod<&UbxLog::Prev>("prev"),
        InstanceMethod<&UbxLog::Seek>("seek"),
        InstanceMethod<&UbxLog::Get>("get"),
        InstanceMethod<&UbxLog::Close>("close"),
    });

    s_constructor = Napi::Persistent(func);
    s_constructor.SuppressDestruct();

    exports.Set("UbxLog", func);
    return exports;
}

UbxLog::UbxLog(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<UbxLog>(info) {}

UbxLog::~UbxLog() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void UbxLog::setReady(std::unique_ptr<IndexDb> db, std::ifstream&& file) {
    m_db = std::move(db);
    m_file = std::move(file);
    m_cursor = -1;
    m_ready = true;
}

Napi::Value UbxLog::Open(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected file path string").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string path = info[0].As<Napi::String>().Utf8Value();

    // Create a new UbxLog instance
    Napi::Object instance = s_constructor.New({});
    UbxLog* log = Napi::ObjectWrap<UbxLog>::Unwrap(instance);

    // Create deferred promise
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

    // Launch async worker
    auto* worker = new IndexWorker(env, deferred, log, instance, path);
    worker->Queue();

    return deferred.Promise();
}

bool UbxLog::ensureOpen(Napi::Env env) {
    if (m_closed) {
        Napi::Error::New(env, "UbxLog is closed").ThrowAsJavaScriptException();
        return false;
    }
    if (!m_ready) {
        Napi::Error::New(env, "UbxLog is not ready").ThrowAsJavaScriptException();
        return false;
    }
    return true;
}

Napi::Value UbxLog::Count(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    if (info.Length() >= 1 && info[0].IsString()) {
        std::string name = info[0].As<Napi::String>().Utf8Value();
        return Napi::Number::New(env, static_cast<double>(m_db->count(name)));
    }
    return Napi::Number::New(env, static_cast<double>(m_db->count()));
}

Napi::Value UbxLog::MessageTypes(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    auto types = m_db->messageTypes();
    Napi::Array result = Napi::Array::New(env, types.size());
    for (std::size_t i = 0; i < types.size(); ++i) {
        result.Set(static_cast<uint32_t>(i), Napi::String::New(env, types[i]));
    }
    return result;
}

Napi::Value UbxLog::Next(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    MessageEntry entry;
    bool found;

    if (info.Length() >= 1 && info[0].IsString()) {
        std::string name = info[0].As<Napi::String>().Utf8Value();
        found = m_db->next(name, m_cursor, entry);
    } else {
        found = m_db->next(m_cursor, entry);
    }

    if (!found) {
        return env.Null();
    }

    m_cursor = entry.seq;
    return deepParseAt(env, entry);
}

Napi::Value UbxLog::Prev(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    MessageEntry entry;
    bool found;

    if (info.Length() >= 1 && info[0].IsString()) {
        std::string name = info[0].As<Napi::String>().Utf8Value();
        found = m_db->prev(name, m_cursor, entry);
    } else {
        found = m_db->prev(m_cursor, entry);
    }

    if (!found) {
        return env.Null();
    }

    m_cursor = entry.seq;
    return deepParseAt(env, entry);
}

Napi::Value UbxLog::Seek(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected position number").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    int64_t pos = info[0].As<Napi::Number>().Int64Value();

    if (pos < 0) {
        // Seek to end: set cursor past the last message
        m_cursor = m_db->maxSeq() + 1;
    } else {
        // Seek to start: set cursor before all messages
        m_cursor = pos - 1;
    }

    return env.Undefined();
}

Napi::Value UbxLog::Get(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!ensureOpen(env)) return env.Undefined();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected (name: string, ordinal: number)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string name = info[0].As<Napi::String>().Utf8Value();
    int64_t ordinal = info[1].As<Napi::Number>().Int64Value();

    MessageEntry entry;
    if (!m_db->getByOrdinal(name, ordinal, entry)) {
        return env.Null();
    }

    return deepParseAt(env, entry);
}

Napi::Value UbxLog::Close(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_file.is_open()) {
        m_file.close();
    }
    m_closed = true;

    return env.Undefined();
}

Napi::Value UbxLog::deepParseAt(Napi::Env env, const MessageEntry& entry) {
    // Seek to offset and read frame bytes
    m_file.seekg(entry.offset);
    if (!m_file.good()) {
        Napi::Error::New(env, "Failed to seek in file").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::vector<uint8_t> buf(static_cast<std::size_t>(entry.length));
    m_file.read(reinterpret_cast<char*>(buf.data()), entry.length);
    if (!m_file.good()) {
        Napi::Error::New(env, "Failed to read from file").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Use the existing Parser class for deep parsing
    Parser p(&env);
    Napi::Array result = p.parse(buf);
    if (result.Length() > 0) {
        return result.Get(static_cast<uint32_t>(0));
    }
    return env.Null();
}
