#include "index_worker.h"
#include "ubx_log.h"
#include <array>

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
    // Open the .ubx file for reading (needed for deep parse later)
    m_file.open(m_path, std::ios::binary);
    if (!m_file.is_open()) {
        SetError("Failed to open file: " + m_path);
        return;
    }

    std::string idxPath = IndexDb::indexPath(m_path);

    if (IndexDb::indexIsFresh(m_path)) {
        // Companion index exists and is up-to-date — skip indexing
        m_db = std::make_unique<IndexDb>(idxPath);
        m_db->loadTypeMap();
        return;
    }

    // Build index from scratch
    m_db = std::make_unique<IndexDb>(idxPath);
    m_db->beginBulkInsert();

    ShallowParser parser;
    constexpr std::size_t CHUNK_SIZE = 65536;
    std::array<char, CHUNK_SIZE> chunk;
    int64_t fileOffset = 0;

    while (m_file.read(chunk.data(), CHUNK_SIZE) || m_file.gcount() > 0) {
        auto bytesRead = static_cast<std::size_t>(m_file.gcount());

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

    // Reset file stream for subsequent deep-parse reads
    m_file.clear();
}

void IndexWorker::OnOK() {
    m_log->setReady(std::move(m_db), std::move(m_file));
    m_deferred.Resolve(m_logRef.Value());
}

void IndexWorker::OnError(const Napi::Error& error) {
    m_deferred.Reject(error.Value());
}
