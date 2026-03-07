#include "index_db.h"
#include <stdexcept>
#include <sys/stat.h>

IndexDb::IndexDb(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to open SQLite: ") + sqlite3_errmsg(m_db));
    }
}

IndexDb::~IndexDb() {
    if (m_insertStmt) sqlite3_finalize(m_insertStmt);
    if (m_insertTypeStmt) sqlite3_finalize(m_insertTypeStmt);
    for (auto& stmt : m_queryStmts) {
        if (stmt) sqlite3_finalize(stmt);
    }
    if (m_db) sqlite3_close(m_db);
}

IndexDb::IndexDb(IndexDb&& other) noexcept
    : m_db(other.m_db)
    , m_insertStmt(other.m_insertStmt)
    , m_insertTypeStmt(other.m_insertTypeStmt)
    , m_nextSeq(other.m_nextSeq)
    , m_nextTypeId(other.m_nextTypeId)
    , m_typeMap(std::move(other.m_typeMap))
    , m_queryStmts(other.m_queryStmts)
{
    other.m_db = nullptr;
    other.m_insertStmt = nullptr;
    other.m_insertTypeStmt = nullptr;
    other.m_queryStmts.fill(nullptr);
}

IndexDb& IndexDb::operator=(IndexDb&& other) noexcept {
    if (this != &other) {
        if (m_insertStmt) sqlite3_finalize(m_insertStmt);
        if (m_insertTypeStmt) sqlite3_finalize(m_insertTypeStmt);
        for (auto& stmt : m_queryStmts) {
            if (stmt) sqlite3_finalize(stmt);
        }
        if (m_db) sqlite3_close(m_db);
        m_db = other.m_db;
        m_insertStmt = other.m_insertStmt;
        m_insertTypeStmt = other.m_insertTypeStmt;
        m_nextSeq = other.m_nextSeq;
        m_nextTypeId = other.m_nextTypeId;
        m_typeMap = std::move(other.m_typeMap);
        m_queryStmts = other.m_queryStmts;
        other.m_db = nullptr;
        other.m_insertStmt = nullptr;
        other.m_insertTypeStmt = nullptr;
        other.m_queryStmts.fill(nullptr);
    }
    return *this;
}

std::string IndexDb::indexPath(const std::string& ubxPath) {
    return ubxPath + ".idx";
}

bool IndexDb::indexIsFresh(const std::string& ubxPath) {
    std::string idxPath = indexPath(ubxPath);
    struct stat ubxStat, idxStat;
    if (stat(ubxPath.c_str(), &ubxStat) != 0) return false;
    if (stat(idxPath.c_str(), &idxStat) != 0) return false;
    return idxStat.st_mtime >= ubxStat.st_mtime;
}

void IndexDb::createSchema() {
    exec("CREATE TABLE IF NOT EXISTS message_types ("
         "  type_id INTEGER PRIMARY KEY,"
         "  name    TEXT NOT NULL UNIQUE"
         ")");
    exec("CREATE TABLE IF NOT EXISTS messages ("
         "  seq     INTEGER PRIMARY KEY,"
         "  type_id INTEGER NOT NULL,"
         "  offset  INTEGER NOT NULL,"
         "  length  INTEGER NOT NULL"
         ")");
}

void IndexDb::loadTypeMap() {
    m_typeMap.clear();
    m_nextTypeId = 0;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db, "SELECT type_id, name FROM message_types", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t typeId = sqlite3_column_int64(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m_typeMap[name] = typeId;
        if (typeId >= m_nextTypeId) m_nextTypeId = typeId + 1;
    }
    sqlite3_finalize(stmt);

    // Also figure out next seq
    m_nextSeq = 0;
    sqlite3_prepare_v2(m_db, "SELECT MAX(seq) FROM messages", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        m_nextSeq = sqlite3_column_int64(stmt, 0) + 1;
    }
    sqlite3_finalize(stmt);

    prepareQueryStatements();
}

void IndexDb::prepareQueryStatements() {
    auto prepare = [this](QueryStmt id, const char* sql) {
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &m_queryStmts[id], nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("Failed to prepare query: ") + sqlite3_errmsg(m_db));
        }
    };

    prepare(CountAll,
        "SELECT COUNT(*) FROM messages");
    prepare(CountByType,
        "SELECT COUNT(*) FROM messages WHERE type_id = ?");
    prepare(MessageTypes,
        "SELECT name FROM message_types ORDER BY name");
    prepare(NextAll,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.seq > ? ORDER BY m.seq LIMIT 1");
    prepare(NextByType,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.type_id = ? AND m.seq > ? ORDER BY m.seq LIMIT 1");
    prepare(PrevAll,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.seq < ? ORDER BY m.seq DESC LIMIT 1");
    prepare(PrevByType,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.type_id = ? AND m.seq < ? ORDER BY m.seq DESC LIMIT 1");
    prepare(GetByOrdinal,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.type_id = ? ORDER BY m.seq LIMIT 1 OFFSET ?");
    prepare(GetBySeq,
        "SELECT m.seq, t.name, m.offset, m.length FROM messages m "
        "JOIN message_types t USING(type_id) WHERE m.seq = ?");
    prepare(MinSeq,
        "SELECT MIN(seq) FROM messages");
    prepare(MaxSeq,
        "SELECT MAX(seq) FROM messages");
}

void IndexDb::prepareStatements() {
    int rc = sqlite3_prepare_v2(m_db,
        "INSERT INTO messages (seq, type_id, offset, length) VALUES (?, ?, ?, ?)",
        -1, &m_insertStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare insert: ") + sqlite3_errmsg(m_db));
    }

    rc = sqlite3_prepare_v2(m_db,
        "INSERT INTO message_types (type_id, name) VALUES (?, ?)",
        -1, &m_insertTypeStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare type insert: ") + sqlite3_errmsg(m_db));
    }
}

int64_t IndexDb::resolveTypeId(const std::string& name) {
    auto it = m_typeMap.find(name);
    if (it != m_typeMap.end()) return it->second;

    int64_t typeId = m_nextTypeId++;
    m_typeMap[name] = typeId;

    sqlite3_reset(m_insertTypeStmt);
    sqlite3_bind_int64(m_insertTypeStmt, 1, typeId);
    sqlite3_bind_text(m_insertTypeStmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(m_insertTypeStmt);

    return typeId;
}

int64_t IndexDb::lookupTypeId(const std::string& name) const {
    auto it = m_typeMap.find(name);
    if (it != m_typeMap.end()) return it->second;
    return -1; // not found
}

void IndexDb::beginBulkInsert() {
    // Aggressive pragmas for bulk loading
    exec("PRAGMA journal_mode = OFF");
    exec("PRAGMA synchronous = OFF");
    exec("PRAGMA cache_size = -65536"); // 64MB
    exec("PRAGMA locking_mode = EXCLUSIVE");
    exec("PRAGMA temp_store = MEMORY");

    createSchema();
    prepareStatements();
    exec("BEGIN TRANSACTION");
}

void IndexDb::insert(const std::string& name, int64_t offset, int32_t length) {
    int64_t typeId = resolveTypeId(name);
    sqlite3_reset(m_insertStmt);
    sqlite3_bind_int64(m_insertStmt, 1, m_nextSeq++);
    sqlite3_bind_int64(m_insertStmt, 2, typeId);
    sqlite3_bind_int64(m_insertStmt, 3, offset);
    sqlite3_bind_int(m_insertStmt, 4, length);
    sqlite3_step(m_insertStmt);
}

void IndexDb::finalizeIndex() {
    exec("COMMIT");

    // Create index AFTER bulk insert — much faster than maintaining during inserts
    exec("CREATE INDEX IF NOT EXISTS idx_type_seq ON messages(type_id, seq)");

    // Reset to safe defaults for query mode
    exec("PRAGMA synchronous = NORMAL");
    exec("PRAGMA journal_mode = WAL");

    prepareQueryStatements();
}

void IndexDb::exec(const char* sql) const {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

MessageEntry IndexDb::readRow(sqlite3_stmt* stmt) {
    MessageEntry e;
    e.seq = sqlite3_column_int64(stmt, 0);
    e.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    e.offset = sqlite3_column_int64(stmt, 2);
    e.length = sqlite3_column_int(stmt, 3);
    return e;
}

int64_t IndexDb::count() const {
    auto* stmt = m_queryStmts[CountAll];
    sqlite3_reset(stmt);
    sqlite3_step(stmt);
    return sqlite3_column_int64(stmt, 0);
}

int64_t IndexDb::count(const std::string& name) const {
    int64_t typeId = lookupTypeId(name);
    if (typeId < 0) return 0;
    auto* stmt = m_queryStmts[CountByType];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, typeId);
    sqlite3_step(stmt);
    return sqlite3_column_int64(stmt, 0);
}

std::vector<std::string> IndexDb::messageTypes() const {
    auto* stmt = m_queryStmts[MessageTypes];
    sqlite3_reset(stmt);
    std::vector<std::string> types;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        types.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    return types;
}

bool IndexDb::next(int64_t afterSeq, MessageEntry& entry) const {
    auto* stmt = m_queryStmts[NextAll];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, afterSeq);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

bool IndexDb::next(const std::string& name, int64_t afterSeq, MessageEntry& entry) const {
    int64_t typeId = lookupTypeId(name);
    if (typeId < 0) return false;
    auto* stmt = m_queryStmts[NextByType];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, typeId);
    sqlite3_bind_int64(stmt, 2, afterSeq);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

bool IndexDb::prev(int64_t beforeSeq, MessageEntry& entry) const {
    auto* stmt = m_queryStmts[PrevAll];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, beforeSeq);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

bool IndexDb::prev(const std::string& name, int64_t beforeSeq, MessageEntry& entry) const {
    int64_t typeId = lookupTypeId(name);
    if (typeId < 0) return false;
    auto* stmt = m_queryStmts[PrevByType];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, typeId);
    sqlite3_bind_int64(stmt, 2, beforeSeq);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

bool IndexDb::getByOrdinal(const std::string& name, int64_t ordinal, MessageEntry& entry) const {
    int64_t typeId = lookupTypeId(name);
    if (typeId < 0) return false;
    auto* stmt = m_queryStmts[GetByOrdinal];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, typeId);
    sqlite3_bind_int64(stmt, 2, ordinal);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

bool IndexDb::getBySeq(int64_t seq, MessageEntry& entry) const {
    auto* stmt = m_queryStmts[GetBySeq];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, seq);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) entry = readRow(stmt);
    return found;
}

int64_t IndexDb::minSeq() const {
    auto* stmt = m_queryStmts[MinSeq];
    sqlite3_reset(stmt);
    int64_t result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        result = sqlite3_column_int64(stmt, 0);
    }
    return result;
}

int64_t IndexDb::maxSeq() const {
    auto* stmt = m_queryStmts[MaxSeq];
    sqlite3_reset(stmt);
    int64_t result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        result = sqlite3_column_int64(stmt, 0);
    }
    return result;
}
