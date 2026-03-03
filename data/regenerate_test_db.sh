#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
db_path="$script_dir/comprehensive_test.db"
sql_path="$script_dir/comprehensive_test.sql"

rm -f "$db_path"
sqlite3 "$db_path" < "$sql_path"
sqlite3 "$db_path" 'PRAGMA optimize;'
