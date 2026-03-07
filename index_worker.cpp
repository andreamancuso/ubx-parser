#include "index_worker.h"
#include "ubx_log.h"
#include <array>
#include <fstream>

IndexWorker::IndexWorker(Napi::Env env,
                         Napi::Promise::Deferred deferred,
                         UbxLog* log,
                         Napi::Object logRef,
                         const std::string& path)
    : Napi::AsyncWorker(env)
    , m_deferred(deferred)
    , m_log(log)
    , m_path(path)
{
    m_logRef = Napi::Persistent(logRef);
}

void IndexWorker::Execute() {
    std::string idxPath = IndexDb::indexPath(m_path);

    if (IndexDb::indexIsFresh(m_path)) {
        // Companion index exists and is up-to-date — skip indexing
        m_db = std::make_unique<IndexDb>(idxPath);
        m_db->loadTypeMap();
        return;
    }

    // Open the .ubx file for indexing
    std::ifstream file(m_path, std::ios::binary);
    if (!file.is_open()) {
        SetError("Failed to open file: " + m_path);
        return;
    }

    // Build index from scratch
    m_db = std::make_unique<IndexDb>(idxPath);
    m_db->beginBulkInsert();

    ShallowParser parser;
    constexpr std::size_t CHUNK_SIZE = 65536;
    std::array<char, CHUNK_SIZE> chunk;
    int64_t fileOffset = 0;

    while (file.read(chunk.data(), CHUNK_SIZE) || file.gcount() > 0) {
        auto bytesRead = static_cast<std::size_t>(file.gcount());

        parser.feed(
            reinterpret_cast<const uint8_t*>(chunk.data()),
            bytesRead,
            fileOffset,
            [this](const FrameInfo& info) {
                m_db->insert(info.name, info.offset, info.length);
            }
        );

        fileOffset += static_cast<int64_t>(bytesRead);
    }

    m_db->finalizeIndex();
}

void IndexWorker::OnOK() {
    m_log->setReady(std::move(m_db), m_path);
    m_deferred.Resolve(m_logRef.Value());
}

void IndexWorker::OnError(const Napi::Error& error) {
    m_deferred.Reject(error.Value());
}
