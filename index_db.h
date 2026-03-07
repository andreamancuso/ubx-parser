#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "sqlite3.h"

struct MessageEntry {
    int64_t seq;
    std::string name;
    int64_t offset;
    int32_t length;
};

class IndexDb {
public:
    // Open or create a file-backed SQLite database
    explicit IndexDb(const std::string& dbPath);
    ~IndexDb();

    // Non-copyable
    IndexDb(const IndexDb&) = delete;
    IndexDb& operator=(const IndexDb&) = delete;

    // Move-friendly for transferring from worker to main thread
    IndexDb(IndexDb&& other) noexcept;
    IndexDb& operator=(IndexDb&& other) noexcept;

    // Companion index file helpers
    static std::string indexPath(const std::string& ubxPath);
    static bool indexIsFresh(const std::string& ubxPath);

    // Load type map from existing index (when reusing a companion .idx file)
    void loadTypeMap();

    // Bulk insert mode
    void beginBulkInsert();
    void insert(const std::string& name, int64_t offset, int32_t length);
    void finalizeIndex(); // commits, creates deferred indexes, resets pragmas

    // Query: total count or count by name
    int64_t count() const;
    int64_t count(const std::string& name) const;

    // Query: distinct message type names
    std::vector<std::string> messageTypes() const;

    // Cursor navigation
    bool next(int64_t afterSeq, MessageEntry& entry) const;
    bool next(const std::string& name, int64_t afterSeq, MessageEntry& entry) const;
    bool prev(int64_t beforeSeq, MessageEntry& entry) const;
    bool prev(const std::string& name, int64_t beforeSeq, MessageEntry& entry) const;

    // Get the Nth message of a given type (0-based)
    bool getByOrdinal(const std::string& name, int64_t ordinal, MessageEntry& entry) const;

    // Get by exact seq
    bool getBySeq(int64_t seq, MessageEntry& entry) const;

    // Min/max seq
    int64_t minSeq() const;
    int64_t maxSeq() const;

private:
    sqlite3* m_db = nullptr;
    sqlite3_stmt* m_insertStmt = nullptr;
    sqlite3_stmt* m_insertTypeStmt = nullptr;
    int64_t m_nextSeq = 0;
    int64_t m_nextTypeId = 0;

    // In-memory name→type_id cache (only ~176 possible UBX types)
    std::unordered_map<std::string, int64_t> m_typeMap;

    void createSchema();
    void prepareStatements();
    int64_t resolveTypeId(const std::string& name);
    int64_t lookupTypeId(const std::string& name) const;
    void exec(const char* sql) const;
    static MessageEntry readRow(sqlite3_stmt* stmt);
};
