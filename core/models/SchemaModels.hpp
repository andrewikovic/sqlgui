#pragma once

#include <string>
#include <vector>

namespace sqlgui::core {

struct TableSummary {
    std::string schema;
    std::string name;
};

struct ColumnInfo {
    std::string name;
    std::string type;
    bool nullable {true};
    bool primary_key {false};
};

struct IndexInfo {
    std::string name;
    bool unique {false};
    std::vector<std::string> columns;
};

struct ForeignKeyInfo {
    std::string name;
    std::string from_column;
    std::string target_schema;
    std::string target_table;
    std::string target_column;
    std::string on_update;
    std::string on_delete;
};

struct TableMetadata {
    std::vector<ColumnInfo> columns;
    std::vector<IndexInfo> indexes;
    std::vector<ForeignKeyInfo> foreign_keys;
};

}  // namespace sqlgui::core
