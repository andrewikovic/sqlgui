#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace sqlgui::core {

enum class DatabaseKind {
    SQLite,
    PostgreSQL,
};

struct ConnectionConfig {
    DatabaseKind kind {DatabaseKind::SQLite};
    std::string name {"Local SQLite"};
    std::filesystem::path sqlite_path {"data/comprehensive_test.db"};
    std::string host {"127.0.0.1"};
    std::uint16_t port {5432};
    std::string database {"postgres"};
    std::string user {"postgres"};
    std::string password;
    bool autocommit {true};
};

}  // namespace sqlgui::core
