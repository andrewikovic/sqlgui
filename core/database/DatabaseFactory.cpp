#include "core/database/DatabaseFactory.hpp"

#include <memory>

#include <spdlog/logger.h>

#include "core/database/PostgresDatabase.hpp"
#include "core/database/SQLiteDatabase.hpp"
#include "core/schema/PostgresSchemaProvider.hpp"
#include "core/schema/SQLiteSchemaProvider.hpp"

namespace sqlgui::core {

Expected<DatabaseSession, DatabaseError> DatabaseFactory::create_session(
    const ConnectionConfig& config,
    const std::shared_ptr<spdlog::logger>& logger) {
    if (config.kind == DatabaseKind::SQLite) {
        auto database = std::make_shared<SQLiteDatabase>(logger);
        if (auto opened = database->open(config); !opened) {
            return make_unexpected(opened.error());
        }

        auto schema_database = std::make_shared<SQLiteDatabase>(logger);
        if (auto opened = schema_database->open(config); !opened) {
            return make_unexpected(opened.error());
        }

        DatabaseSession session;
        session.database = database;
        session.schema_provider = std::make_unique<SQLiteSchemaProvider>(schema_database, logger);
        return session;
    }

#if SQLGUI_HAS_POSTGRES
    auto database = std::make_shared<PostgresDatabase>(logger);
    if (auto opened = database->open(config); !opened) {
        return make_unexpected(opened.error());
    }

    auto schema_database = std::make_shared<PostgresDatabase>(logger);
    if (auto opened = schema_database->open(config); !opened) {
        return make_unexpected(opened.error());
    }

    DatabaseSession session;
    session.database = database;
    session.schema_provider = std::make_unique<PostgresSchemaProvider>(schema_database, logger);
    return session;
#else
    (void)logger;
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support was not enabled when this binary was built",
    });
#endif
}

}  // namespace sqlgui::core
