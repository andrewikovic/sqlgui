#pragma once

#include <memory>

#include "core/common/Error.hpp"
#include "core/common/Expected.hpp"
#include "core/models/ConnectionConfig.hpp"

namespace spdlog {
class logger;
}

namespace sqlgui::core {

class Database;
class SchemaProvider;

struct DatabaseSession {
    std::shared_ptr<Database> database;
    std::unique_ptr<SchemaProvider> schema_provider;
};

class DatabaseFactory {
public:
    [[nodiscard]] static Expected<DatabaseSession, DatabaseError> create_session(
        const ConnectionConfig& config,
        const std::shared_ptr<spdlog::logger>& logger);
};

}  // namespace sqlgui::core
