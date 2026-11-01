/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Lua/lua_database.h"

#include <mariadb/mysql.h>
#include <sqlite3.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"

namespace lua::bindings {

namespace {

constexpr unsigned int kDefaultMySqlTimeoutSeconds = 10;
constexpr int kDefaultSqliteBusyTimeoutMs = 5000;
constexpr std::size_t kMySqlColumnBufferSize = 256;

enum class ParameterType {
  Null,
  Integer,
  Number,
  Boolean,
  String,
};

struct QueryParameter {
  ParameterType type = ParameterType::Null;
  std::int64_t integer = 0;
  double number = 0.0;
  bool boolean = false;
  std::string string;
};

using LuaCallResult = std::tuple<sol::object, sol::object>;

sol::table MakeError(sol::state_view lua, std::string_view driver, std::string_view message, std::uint64_t code = 0,
                     std::string_view sql_state = {}) {
  sol::table error = lua.create_table();
  error["driver"] = driver;
  error["message"] = message;
  error["code"] = code;
  if (!sql_state.empty()) {
    error["sqlState"] = sql_state;
  }
  return error;
}

LuaCallResult Failure(sol::state_view lua, std::string_view driver, std::string_view message, std::uint64_t code = 0,
                      std::string_view sql_state = {}) {
  return {sol::make_object(lua, sol::lua_nil), sol::make_object(lua, MakeError(lua, driver, message, code, sql_state))};
}

LuaCallResult Success(sol::state_view lua, const sol::object& value) {
  return {value, sol::make_object(lua, sol::lua_nil)};
}

sol::object GetOption(const sol::table& options, const char* name) {
  return options[name];
}

bool ReadStringOption(const sol::table& options, const char* name, bool required, std::string default_value, std::string& value, std::string& error) {
  sol::object option = GetOption(options, name);
  if (!option.valid() || option.get_type() == sol::type::nil) {
    if (required) {
      error = std::string("Missing required '") + name + "' option";
      return false;
    }
    value = std::move(default_value);
    return true;
  }
  if (option.get_type() != sol::type::string) {
    error = std::string("Option '") + name + "' must be a string";
    return false;
  }
  value = option.as<std::string>();
  if (required && value.empty()) {
    error = std::string("Option '") + name + "' must not be empty";
    return false;
  }
  return true;
}

bool ReadBooleanOption(const sol::table& options, const char* name, bool default_value, bool& value, std::string& error) {
  sol::object option = GetOption(options, name);
  if (!option.valid() || option.get_type() == sol::type::nil) {
    value = default_value;
    return true;
  }
  if (option.get_type() != sol::type::boolean) {
    error = std::string("Option '") + name + "' must be a boolean";
    return false;
  }
  value = option.as<bool>();
  return true;
}

bool ReadIntegerOption(const sol::table& options, const char* name, std::uint64_t default_value, std::uint64_t min_value, std::uint64_t max_value,
                       std::uint64_t& value, std::string& error) {
  sol::object option = GetOption(options, name);
  if (!option.valid() || option.get_type() == sol::type::nil) {
    value = default_value;
    return true;
  }
  if (option.get_type() != sol::type::number) {
    error = std::string("Option '") + name + "' must be an integer";
    return false;
  }

  const double number = option.as<double>();
  if (!std::isfinite(number) || std::floor(number) != number || number < static_cast<double>(min_value) || number > static_cast<double>(max_value)) {
    error = std::string("Option '") + name + "' is outside its valid range";
    return false;
  }
  value = static_cast<std::uint64_t>(number);
  return true;
}

bool IsLuaInteger(const sol::object& value) {
  lua_State* lua = value.lua_state();
  sol::stack::push(lua, value);
  const bool is_integer = lua_isinteger(lua, -1) != 0;
  lua_pop(lua, 1);
  return is_integer;
}

bool DecodeParameters(const sol::variadic_args& arguments, std::vector<QueryParameter>& parameters, std::string& error) {
  parameters.clear();
  parameters.reserve(arguments.size());

  std::size_t index = 0;
  for (const auto& argument : arguments) {
    ++index;
    sol::object value = argument;
    QueryParameter parameter;

    switch (value.get_type()) {
      case sol::type::nil:
        parameter.type = ParameterType::Null;
        break;
      case sol::type::boolean:
        parameter.type = ParameterType::Boolean;
        parameter.boolean = value.as<bool>();
        break;
      case sol::type::number:
        if (IsLuaInteger(value)) {
          parameter.type = ParameterType::Integer;
          parameter.integer = value.as<std::int64_t>();
        } else {
          parameter.type = ParameterType::Number;
          parameter.number = value.as<double>();
        }
        break;
      case sol::type::string:
        parameter.type = ParameterType::String;
        parameter.string = value.as<std::string>();
        break;
      default:
        error = "SQL parameter " + std::to_string(index) + " must be nil, boolean, number, or string";
        return false;
    }

    parameters.push_back(std::move(parameter));
  }
  return true;
}

void AddColumnName(sol::table& columns, std::size_t index, const char* name) {
  columns[static_cast<int>(index + 1)] = name ? name : "";
}

void SetUnsignedResultValue(sol::table& result, const char* name, std::uint64_t value) {
  constexpr auto kLuaIntegerMax = static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max());
  if (value <= kLuaIntegerMax) {
    result[name] = static_cast<lua_Integer>(value);
  } else {
    result[name] = std::to_string(value);
  }
}

sol::table MakeQueryResult(sol::state_view lua, sol::table rows, sol::table columns, std::uint64_t affected_rows, std::uint64_t last_insert_id) {
  sol::table result = lua.create_table();
  result["rows"] = std::move(rows);
  result["columns"] = std::move(columns);
  SetUnsignedResultValue(result, "affectedRows", affected_rows);
  SetUnsignedResultValue(result, "lastInsertId", last_insert_id);
  return result;
}

/* luagmp (class)
 *
 * Synchronous SQLite database connection. `SQLite.open()` returns a connection
 * or nil plus a structured error. `query()` and `execute()` pass the SQL statement
 * unchanged to SQLite's prepared-statement API and bind positional ? parameters.
 * Successful results contain rows, columns, affectedRows, and lastInsertId.
 * Errors contain driver, code, message, and sqlState when available.
 *
 * @version  0.3.0
 * @name     SQLite
 * @side     server
 * @category Database
 *
 */
class SQLiteConnection {
public:
  explicit SQLiteConnection(sqlite3* handle) : handle_(handle) {
  }

  ~SQLiteConnection() {
    Close();
  }

  SQLiteConnection(const SQLiteConnection&) = delete;
  SQLiteConnection& operator=(const SQLiteConnection&) = delete;

  static LuaCallResult Open(const sol::table& options, sol::this_state ts);

  /* luagmp (method)
   *
   * Close this connection. Connections are also closed during garbage collection.
   *
   * @name     close
   * @return   (boolean) True when an open connection was closed.
   *
   */
  bool Close() {
    if (!handle_) {
      return false;
    }
    sqlite3_close_v2(handle_);
    handle_ = nullptr;
    return true;
  }

  /* luagmp (method)
   *
   * Return whether this connection has not been closed.
   *
   * @name     isOpen
   * @return   (boolean)
   *
   */
  bool IsOpen() const {
    return handle_ != nullptr;
  }

  /* luagmp (method)
   *
   * Execute one SQL statement. The statement is passed unchanged to SQLite and
   * positional ? markers are bound to the remaining arguments. Lua nil binds SQL
   * NULL; SQL NULL is returned as nil, so use the columns array when column
   * presence must be distinguished from a missing row key.
   *
   * @name     query
   * @param    (string) sql      SQL statement.
   * @param    (...) parameters  Positional nil, boolean, number, or string values.
   * @return   (table|nil, table|nil) Result and nil, or nil and an error table.
   *
   */
  LuaCallResult Query(const std::string& sql, sol::variadic_args arguments, sol::this_state ts) {
    sol::state_view lua(ts);
    if (!handle_) {
      return Failure(lua, "sqlite", "SQLite connection is closed");
    }
    if (sql.empty()) {
      return Failure(lua, "sqlite", "SQL statement must not be empty");
    }
    if (sql.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return Failure(lua, "sqlite", "SQL statement is too large");
    }

    std::vector<QueryParameter> parameters;
    std::string parameter_error;
    if (!DecodeParameters(arguments, parameters, parameter_error)) {
      return Failure(lua, "sqlite", parameter_error);
    }

    sqlite3_stmt* raw_statement = nullptr;
    const char* tail = nullptr;
    const int prepare_result = sqlite3_prepare_v2(handle_, sql.data(), static_cast<int>(sql.size()), &raw_statement, &tail);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw_statement, &sqlite3_finalize);
    if (prepare_result != SQLITE_OK) {
      return Failure(lua, "sqlite", sqlite3_errmsg(handle_), static_cast<std::uint64_t>(prepare_result));
    }
    if (!statement) {
      return Failure(lua, "sqlite", "SQL statement did not contain an executable query");
    }
    while (tail && *tail != '\0') {
      sqlite3_stmt* raw_extra_statement = nullptr;
      const char* next_tail = nullptr;
      const int tail_result = sqlite3_prepare_v2(handle_, tail, -1, &raw_extra_statement, &next_tail);
      std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> extra_statement(raw_extra_statement, &sqlite3_finalize);
      if (tail_result != SQLITE_OK) {
        return Failure(lua, "sqlite", sqlite3_errmsg(handle_), static_cast<std::uint64_t>(tail_result));
      }
      if (extra_statement) {
        return Failure(lua, "sqlite", "Only one SQL statement can be executed at a time");
      }
      if (!next_tail || next_tail == tail) {
        break;
      }
      tail = next_tail;
    }

    const int expected_parameter_count = sqlite3_bind_parameter_count(statement.get());
    if (expected_parameter_count != static_cast<int>(parameters.size())) {
      return Failure(lua, "sqlite",
                     "SQL statement expects " + std::to_string(expected_parameter_count) + " parameters, but " + std::to_string(parameters.size()) +
                         " were provided",
                     SQLITE_RANGE);
    }

    for (std::size_t index = 0; index < parameters.size(); ++index) {
      const QueryParameter& parameter = parameters[index];
      const int sqlite_index = static_cast<int>(index + 1);
      int bind_result = SQLITE_OK;
      switch (parameter.type) {
        case ParameterType::Null:
          bind_result = sqlite3_bind_null(statement.get(), sqlite_index);
          break;
        case ParameterType::Integer:
          bind_result = sqlite3_bind_int64(statement.get(), sqlite_index, parameter.integer);
          break;
        case ParameterType::Number:
          bind_result = sqlite3_bind_double(statement.get(), sqlite_index, parameter.number);
          break;
        case ParameterType::Boolean:
          bind_result = sqlite3_bind_int(statement.get(), sqlite_index, parameter.boolean ? 1 : 0);
          break;
        case ParameterType::String:
          if (parameter.string.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return Failure(lua, "sqlite", "SQL string parameter is too large", SQLITE_TOOBIG);
          }
          bind_result =
              sqlite3_bind_text(statement.get(), sqlite_index, parameter.string.data(), static_cast<int>(parameter.string.size()), SQLITE_TRANSIENT);
          break;
      }
      if (bind_result != SQLITE_OK) {
        return Failure(lua, "sqlite", sqlite3_errmsg(handle_), static_cast<std::uint64_t>(bind_result));
      }
    }

    const int column_count = sqlite3_column_count(statement.get());
    sol::table columns = lua.create_table(column_count, 0);
    for (int index = 0; index < column_count; ++index) {
      AddColumnName(columns, static_cast<std::size_t>(index), sqlite3_column_name(statement.get(), index));
    }

    const sqlite3_int64 previous_insert_id = sqlite3_last_insert_rowid(handle_);
    const sqlite3_int64 previous_total_changes = sqlite3_total_changes64(handle_);
    sol::table rows = lua.create_table();
    int row_index = 1;
    int step_result = SQLITE_OK;
    while ((step_result = sqlite3_step(statement.get())) == SQLITE_ROW) {
      sol::table row = lua.create_table();
      for (int column_index = 0; column_index < column_count; ++column_index) {
        const char* column_name = sqlite3_column_name(statement.get(), column_index);
        if (!column_name) {
          continue;
        }

        switch (sqlite3_column_type(statement.get(), column_index)) {
          case SQLITE_INTEGER:
            row[column_name] = static_cast<lua_Integer>(sqlite3_column_int64(statement.get(), column_index));
            break;
          case SQLITE_FLOAT:
            row[column_name] = sqlite3_column_double(statement.get(), column_index);
            break;
          case SQLITE_TEXT: {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), column_index));
            const int size = sqlite3_column_bytes(statement.get(), column_index);
            row[column_name] = std::string(text ? text : "", static_cast<std::size_t>(size));
            break;
          }
          case SQLITE_BLOB: {
            const auto* bytes = static_cast<const char*>(sqlite3_column_blob(statement.get(), column_index));
            const int size = sqlite3_column_bytes(statement.get(), column_index);
            row[column_name] = std::string(bytes ? bytes : "", static_cast<std::size_t>(size));
            break;
          }
          case SQLITE_NULL:
            row[column_name] = sol::lua_nil;
            break;
          default:
            row[column_name] = sol::lua_nil;
            break;
        }
      }
      rows[row_index++] = std::move(row);
    }

    if (step_result != SQLITE_DONE) {
      return Failure(lua, "sqlite", sqlite3_errmsg(handle_), static_cast<std::uint64_t>(step_result));
    }

    const sqlite3_int64 current_total_changes = sqlite3_total_changes64(handle_);
    const std::uint64_t affected_rows =
        current_total_changes > previous_total_changes ? static_cast<std::uint64_t>(current_total_changes - previous_total_changes) : 0;
    const sqlite3_int64 current_insert_id = sqlite3_last_insert_rowid(handle_);
    const std::uint64_t last_insert_id = current_insert_id != previous_insert_id ? static_cast<std::uint64_t>(current_insert_id) : 0;
    return Success(lua, sol::make_object(lua, MakeQueryResult(lua, std::move(rows), std::move(columns), affected_rows, last_insert_id)));
  }

  /* luagmp (method)
   *
   * Executes a statement intended for commands which do not return rows. Uses the parameter and result contract of `query`.
   *
   * @name     execute
   * @param    (string) sql      SQL statement.
   * @param    (...) parameters  Positional SQL parameter values.
   * @return   (table|nil, table|nil) Result and nil, or nil and an error table.
   *
   */
  LuaCallResult Execute(const std::string& sql, sol::variadic_args arguments, sol::this_state ts) {
    return Query(sql, arguments, ts);
  }

private:
  sqlite3* handle_ = nullptr;
};

/* luagmp (method)
 *
 * Open an SQLite database under the server's data/internal directory. The
 * database option must be relative and cannot contain parent traversal. Use
 * :memory: for an in-memory database. Missing parent directories are created
 * when create is enabled. Foreign-key enforcement is enabled by default.
 *
 * @name     open
 * @param    (table) options  database, readOnly=false, create=true, foreignKeys=true, busyTimeout=5000.
 * @return   (SQLite|nil, table|nil) Connection and nil, or nil and an error table.
 *
 * @static
 */
LuaCallResult SQLiteConnection::Open(const sol::table& options, sol::this_state ts) {
  sol::state_view lua(ts);
  std::string database;
  std::string error;
  bool read_only = false;
  bool create = true;
  bool foreign_keys = true;
  std::uint64_t busy_timeout = kDefaultSqliteBusyTimeoutMs;

  if (!ReadStringOption(options, "database", true, {}, database, error) || !ReadBooleanOption(options, "readOnly", false, read_only, error)) {
    return Failure(lua, "sqlite", error);
  }
  if (!ReadBooleanOption(options, "create", !read_only, create, error) || !ReadBooleanOption(options, "foreignKeys", true, foreign_keys, error) ||
      !ReadIntegerOption(options, "busyTimeout", kDefaultSqliteBusyTimeoutMs, 0, static_cast<std::uint64_t>(std::numeric_limits<int>::max()),
                         busy_timeout, error)) {
    return Failure(lua, "sqlite", error);
  }
  if (read_only && create) {
    return Failure(lua, "sqlite", "SQLite options 'readOnly' and 'create' cannot both be true");
  }

  std::string sqlite_path = database;
  if (database != ":memory:") {
    const std::optional<std::filesystem::path> resolved = ::lua::bind_helpers::ResolveDataPath(database, false);
    if (!resolved) {
      return Failure(lua, "sqlite", "Invalid SQLite database path");
    }
    sqlite_path = resolved->string();
  }

  if (create && database != ":memory:") {
    const std::filesystem::path parent = std::filesystem::path(sqlite_path).parent_path();
    if (!parent.empty()) {
      std::error_code filesystem_error;
      std::filesystem::create_directories(parent, filesystem_error);
      if (filesystem_error) {
        return Failure(lua, "sqlite", "Unable to create SQLite database directory: " + filesystem_error.message());
      }
    }
  }

  int flags = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
  if (create) {
    flags |= SQLITE_OPEN_CREATE;
  }

  sqlite3* raw_handle = nullptr;
  const int open_result = sqlite3_open_v2(sqlite_path.c_str(), &raw_handle, flags, nullptr);
  if (open_result != SQLITE_OK) {
    const std::string message = raw_handle ? sqlite3_errmsg(raw_handle) : sqlite3_errstr(open_result);
    if (raw_handle) {
      sqlite3_close_v2(raw_handle);
    }
    return Failure(lua, "sqlite", message, static_cast<std::uint64_t>(open_result));
  }

  std::shared_ptr<SQLiteConnection> connection = std::make_shared<SQLiteConnection>(raw_handle);
  const int timeout_result = sqlite3_busy_timeout(raw_handle, static_cast<int>(busy_timeout));
  if (timeout_result != SQLITE_OK) {
    return Failure(lua, "sqlite", sqlite3_errmsg(raw_handle), static_cast<std::uint64_t>(timeout_result));
  }

  if (foreign_keys) {
    char* raw_error = nullptr;
    const int foreign_key_result = sqlite3_exec(raw_handle, "PRAGMA foreign_keys = ON", nullptr, nullptr, &raw_error);
    if (foreign_key_result != SQLITE_OK) {
      const std::string message = raw_error ? raw_error : sqlite3_errmsg(raw_handle);
      sqlite3_free(raw_error);
      return Failure(lua, "sqlite", message, static_cast<std::uint64_t>(foreign_key_result));
    }
  }

  return Success(lua, sol::make_object(lua, std::move(connection)));
}

struct MySqlParameter {
  MYSQL_BIND bind{};
  std::int64_t integer = 0;
  double number = 0.0;
  signed char boolean = 0;
  std::string string;
  unsigned long length = 0;
  my_bool is_null = 0;
};

struct MySqlColumnBuffer {
  MYSQL_BIND bind{};
  std::array<char, kMySqlColumnBufferSize> bytes{};
  unsigned long length = 0;
  my_bool is_null = 0;
  my_bool truncated = 0;
};

sol::object ConvertMySqlValue(sol::state_view lua, const MYSQL_FIELD& field, const std::string& value) {
  switch (field.type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR:
      if ((field.flags & UNSIGNED_FLAG) != 0) {
        std::uint64_t parsed = 0;
        const auto [end, status] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (status == std::errc{} && end == value.data() + value.size() &&
            parsed <= static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max())) {
          return sol::make_object(lua, static_cast<lua_Integer>(parsed));
        }
      } else {
        std::int64_t parsed = 0;
        const auto [end, status] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (status == std::errc{} && end == value.data() + value.size()) {
          return sol::make_object(lua, static_cast<lua_Integer>(parsed));
        }
      }
      return sol::make_object(lua, value);
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE: {
      double parsed = 0.0;
      const auto [end, status] = std::from_chars(value.data(), value.data() + value.size(), parsed);
      if (status == std::errc{} && end == value.data() + value.size()) {
        return sol::make_object(lua, parsed);
      }
      return sol::make_object(lua, value);
    }
    default:
      return sol::make_object(lua, value);
  }
}

/* luagmp (class)
 *
 * Synchronous MySQL database connection backed by MariaDB Connector/C.
 * `MySQL.open()` returns a connection or nil plus a structured error. `query()`
 * and `execute()` pass the SQL statement unchanged to the connector's prepared-
 * statement API and bind positional ? parameters. Successful results contain
 * rows, columns, affectedRows, and lastInsertId. Errors contain driver, code,
 * message, and sqlState when available.
 *
 * @version  0.3.0
 * @name     MySQL
 * @side     server
 * @category Database
 *
 */
class MySQLConnection {
public:
  explicit MySQLConnection(MYSQL* handle) : handle_(handle) {
  }

  ~MySQLConnection() {
    Close();
  }

  MySQLConnection(const MySQLConnection&) = delete;
  MySQLConnection& operator=(const MySQLConnection&) = delete;

  static LuaCallResult Open(const sol::table& options, sol::this_state ts);

  /* luagmp (method)
   *
   * Close this connection. Connections are also closed during garbage collection.
   *
   * @name     close
   * @return   (boolean) True when an open connection was closed.
   *
   */
  bool Close() {
    if (!handle_) {
      return false;
    }
    mysql_close(handle_);
    handle_ = nullptr;
    return true;
  }

  /* luagmp (method)
   *
   * Return whether this connection has not been closed.
   *
   * @name     isOpen
   * @return   (boolean)
   *
   */
  bool IsOpen() const {
    return handle_ != nullptr;
  }

  /* luagmp (method)
   *
   * Execute one SQL statement. The statement is passed unchanged to the connector
   * and positional ? markers are bound to the remaining arguments. Lua nil binds
   * SQL NULL; SQL NULL is returned as nil, so use the columns array when column
   * presence must be distinguished from a missing row key.
   *
   * @name     query
   * @param    (string) sql      SQL statement.
   * @param    (...) parameters  Positional nil, boolean, number, or string values.
   * @return   (table|nil, table|nil) Result and nil, or nil and an error table.
   *
   */
  LuaCallResult Query(const std::string& sql, sol::variadic_args arguments, sol::this_state ts) {
    sol::state_view lua(ts);
    if (!handle_) {
      return Failure(lua, "mysql", "MySQL connection is closed");
    }
    if (sql.empty()) {
      return Failure(lua, "mysql", "SQL statement must not be empty");
    }
    if (sql.size() > static_cast<std::size_t>(std::numeric_limits<unsigned long>::max())) {
      return Failure(lua, "mysql", "SQL statement is too large");
    }

    std::vector<QueryParameter> parameters;
    std::string parameter_error;
    if (!DecodeParameters(arguments, parameters, parameter_error)) {
      return Failure(lua, "mysql", parameter_error);
    }

    MYSQL_STMT* raw_statement = mysql_stmt_init(handle_);
    if (!raw_statement) {
      return Failure(lua, "mysql", mysql_error(handle_), mysql_errno(handle_), mysql_sqlstate(handle_));
    }
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> statement(raw_statement, &mysql_stmt_close);

    if (mysql_stmt_prepare(statement.get(), sql.data(), static_cast<unsigned long>(sql.size())) != 0) {
      return StatementFailure(lua, statement.get());
    }

    const unsigned long expected_parameter_count = mysql_stmt_param_count(statement.get());
    if (expected_parameter_count != parameters.size()) {
      return Failure(lua, "mysql",
                     "SQL statement expects " + std::to_string(expected_parameter_count) + " parameters, but " + std::to_string(parameters.size()) +
                         " were provided");
    }

    std::vector<MySqlParameter> native_parameters(parameters.size());
    std::vector<MYSQL_BIND> parameter_bindings(parameters.size());
    for (std::size_t index = 0; index < parameters.size(); ++index) {
      const QueryParameter& parameter = parameters[index];
      MySqlParameter& native = native_parameters[index];
      native.bind.is_null = &native.is_null;
      native.bind.length = &native.length;

      switch (parameter.type) {
        case ParameterType::Null:
          native.is_null = 1;
          native.bind.buffer_type = MYSQL_TYPE_NULL;
          break;
        case ParameterType::Integer:
          native.integer = parameter.integer;
          native.bind.buffer_type = MYSQL_TYPE_LONGLONG;
          native.bind.buffer = &native.integer;
          native.bind.buffer_length = sizeof(native.integer);
          break;
        case ParameterType::Number:
          native.number = parameter.number;
          native.bind.buffer_type = MYSQL_TYPE_DOUBLE;
          native.bind.buffer = &native.number;
          native.bind.buffer_length = sizeof(native.number);
          break;
        case ParameterType::Boolean:
          native.boolean = parameter.boolean ? 1 : 0;
          native.bind.buffer_type = MYSQL_TYPE_TINY;
          native.bind.buffer = &native.boolean;
          native.bind.buffer_length = sizeof(native.boolean);
          break;
        case ParameterType::String:
          if (parameter.string.size() > static_cast<std::size_t>(std::numeric_limits<unsigned long>::max())) {
            return Failure(lua, "mysql", "SQL string parameter is too large");
          }
          native.string = parameter.string;
          native.length = static_cast<unsigned long>(native.string.size());
          native.bind.buffer_type = MYSQL_TYPE_STRING;
          native.bind.buffer = native.string.data();
          native.bind.buffer_length = native.length;
          break;
      }
      parameter_bindings[index] = native.bind;
    }

    if (!parameter_bindings.empty() && mysql_stmt_bind_param(statement.get(), parameter_bindings.data()) != 0) {
      return StatementFailure(lua, statement.get());
    }
    if (mysql_stmt_execute(statement.get()) != 0) {
      return StatementFailure(lua, statement.get());
    }

    MYSQL_RES* raw_metadata = mysql_stmt_result_metadata(statement.get());
    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> metadata(raw_metadata, &mysql_free_result);
    const unsigned int column_count = mysql_stmt_field_count(statement.get());
    if (column_count > 0 && !metadata) {
      return StatementFailure(lua, statement.get());
    }

    sol::table rows = lua.create_table();
    sol::table columns = lua.create_table(static_cast<int>(column_count), 0);
    if (metadata) {
      MYSQL_FIELD* fields = mysql_fetch_fields(metadata.get());
      for (unsigned int index = 0; index < column_count; ++index) {
        AddColumnName(columns, index, fields[index].name);
      }

      std::vector<MySqlColumnBuffer> column_buffers(column_count);
      std::vector<MYSQL_BIND> result_bindings(column_count);
      for (unsigned int index = 0; index < column_count; ++index) {
        MySqlColumnBuffer& column = column_buffers[index];
        column.bind.buffer_type = MYSQL_TYPE_STRING;
        column.bind.buffer = column.bytes.data();
        column.bind.buffer_length = static_cast<unsigned long>(column.bytes.size());
        column.bind.length = &column.length;
        column.bind.is_null = &column.is_null;
        column.bind.error = &column.truncated;
        result_bindings[index] = column.bind;
      }

      if (mysql_stmt_bind_result(statement.get(), result_bindings.data()) != 0) {
        return StatementFailure(lua, statement.get());
      }

      int row_index = 1;
      while (true) {
        for (MySqlColumnBuffer& column : column_buffers) {
          column.length = 0;
          column.is_null = 0;
          column.truncated = 0;
        }
        const int fetch_result = mysql_stmt_fetch(statement.get());
        if (fetch_result == MYSQL_NO_DATA) {
          break;
        }
        if (fetch_result != 0 && fetch_result != MYSQL_DATA_TRUNCATED) {
          return StatementFailure(lua, statement.get());
        }

        sol::table row = lua.create_table();
        for (unsigned int column_index = 0; column_index < column_count; ++column_index) {
          MySqlColumnBuffer& column = column_buffers[column_index];
          const char* column_name = fields[column_index].name;
          if (!column_name) {
            continue;
          }
          if (column.is_null) {
            row[column_name] = sol::lua_nil;
            continue;
          }

          std::string value;
          if (column.truncated || column.length > column.bytes.size()) {
            if (column.length == std::numeric_limits<unsigned long>::max()) {
              return Failure(lua, "mysql", "MySQL column value is too large");
            }
            value.resize(static_cast<std::size_t>(column.length) + 1);
            unsigned long fetched_length = 0;
            my_bool is_null = 0;
            my_bool truncated = 0;
            MYSQL_BIND fetch_binding{};
            fetch_binding.buffer_type = MYSQL_TYPE_STRING;
            fetch_binding.buffer = value.data();
            fetch_binding.buffer_length = static_cast<unsigned long>(value.size());
            fetch_binding.length = &fetched_length;
            fetch_binding.is_null = &is_null;
            fetch_binding.error = &truncated;
            if (mysql_stmt_fetch_column(statement.get(), &fetch_binding, column_index, 0) != 0) {
              return StatementFailure(lua, statement.get());
            }
            value.resize(fetched_length);
          } else {
            value.assign(column.bytes.data(), column.length);
          }
          row[column_name] = ConvertMySqlValue(lua, fields[column_index], value);
        }
        rows[row_index++] = std::move(row);
      }
    }

    const my_ulonglong raw_affected_rows = mysql_stmt_affected_rows(statement.get());
    const std::uint64_t affected_rows = raw_affected_rows == static_cast<my_ulonglong>(-1) ? 0 : raw_affected_rows;
    const std::uint64_t last_insert_id = mysql_stmt_insert_id(statement.get());
    return Success(lua, sol::make_object(lua, MakeQueryResult(lua, std::move(rows), std::move(columns), affected_rows, last_insert_id)));
  }

  /* luagmp (method)
   *
   * Executes a statement intended for commands which do not return rows. Uses the parameter and result contract of `query()`.
   *
   * @name     execute
   * @param    (string) sql      SQL statement.
   * @param    (...) parameters  Positional SQL parameter values.
   * @return   (table|nil, table|nil) Result and nil, or nil and an error table.
   *
   */
  LuaCallResult Execute(const std::string& sql, sol::variadic_args arguments, sol::this_state ts) {
    return Query(sql, arguments, ts);
  }

private:
  static LuaCallResult StatementFailure(sol::state_view lua, MYSQL_STMT* statement) {
    return Failure(lua, "mysql", mysql_stmt_error(statement), mysql_stmt_errno(statement), mysql_stmt_sqlstate(statement));
  }

  MYSQL* handle_ = nullptr;
};

/* luagmp (method)
 *
 * Open a MySQL connection. Timeouts are expressed in seconds and default to 10.
 *
 * @name     open
 * @param    (table) options  database, username, password="", host="127.0.0.1", port=3306, charset="utf8mb4",
 *                            unixSocket="", connectTimeout=10, readTimeout=10, writeTimeout=10.
 * @return   (MySQL|nil, table|nil) Connection and nil, or nil and an error table.
 *
 * @static
 */
LuaCallResult MySQLConnection::Open(const sol::table& options, sol::this_state ts) {
  sol::state_view lua(ts);
  std::string host;
  std::string database;
  std::string username;
  std::string password;
  std::string charset;
  std::string unix_socket;
  std::string error;
  std::uint64_t port = 3306;
  std::uint64_t connect_timeout = kDefaultMySqlTimeoutSeconds;
  std::uint64_t read_timeout = kDefaultMySqlTimeoutSeconds;
  std::uint64_t write_timeout = kDefaultMySqlTimeoutSeconds;

  if (!ReadStringOption(options, "host", false, "127.0.0.1", host, error) || !ReadStringOption(options, "database", true, {}, database, error) ||
      !ReadStringOption(options, "username", true, {}, username, error) || !ReadStringOption(options, "password", false, {}, password, error) ||
      !ReadStringOption(options, "charset", false, "utf8mb4", charset, error) ||
      !ReadStringOption(options, "unixSocket", false, {}, unix_socket, error) || !ReadIntegerOption(options, "port", 3306, 1, 65535, port, error) ||
      !ReadIntegerOption(options, "connectTimeout", kDefaultMySqlTimeoutSeconds, 1, std::numeric_limits<unsigned int>::max(), connect_timeout,
                         error) ||
      !ReadIntegerOption(options, "readTimeout", kDefaultMySqlTimeoutSeconds, 1, std::numeric_limits<unsigned int>::max(), read_timeout, error) ||
      !ReadIntegerOption(options, "writeTimeout", kDefaultMySqlTimeoutSeconds, 1, std::numeric_limits<unsigned int>::max(), write_timeout, error)) {
    return Failure(lua, "mysql", error);
  }
  if (host.empty() && unix_socket.empty()) {
    return Failure(lua, "mysql", "MySQL option 'host' must not be empty when 'unixSocket' is not set");
  }
  if (charset.empty()) {
    return Failure(lua, "mysql", "MySQL option 'charset' must not be empty");
  }

  MYSQL* raw_handle = mysql_init(nullptr);
  if (!raw_handle) {
    return Failure(lua, "mysql", "Unable to allocate a MySQL connection");
  }
  std::shared_ptr<MySQLConnection> connection = std::make_shared<MySQLConnection>(raw_handle);

  const unsigned int native_connect_timeout = static_cast<unsigned int>(connect_timeout);
  const unsigned int native_read_timeout = static_cast<unsigned int>(read_timeout);
  const unsigned int native_write_timeout = static_cast<unsigned int>(write_timeout);
  my_bool report_truncation = 1;
  if (mysql_options(raw_handle, MYSQL_OPT_CONNECT_TIMEOUT, &native_connect_timeout) != 0 ||
      mysql_options(raw_handle, MYSQL_OPT_READ_TIMEOUT, &native_read_timeout) != 0 ||
      mysql_options(raw_handle, MYSQL_OPT_WRITE_TIMEOUT, &native_write_timeout) != 0 ||
      mysql_options(raw_handle, MYSQL_SET_CHARSET_NAME, charset.c_str()) != 0 ||
      mysql_options(raw_handle, MYSQL_REPORT_DATA_TRUNCATION, &report_truncation) != 0) {
    return Failure(lua, "mysql", mysql_error(raw_handle), mysql_errno(raw_handle), mysql_sqlstate(raw_handle));
  }

  const char* socket = unix_socket.empty() ? nullptr : unix_socket.c_str();
  if (!mysql_real_connect(raw_handle, host.empty() ? nullptr : host.c_str(), username.c_str(), password.c_str(), database.c_str(),
                          static_cast<unsigned int>(port), socket, 0)) {
    return Failure(lua, "mysql", mysql_error(raw_handle), mysql_errno(raw_handle), mysql_sqlstate(raw_handle));
  }

  return Success(lua, sol::make_object(lua, std::move(connection)));
}

}  // namespace

void BindDatabase(sol::state& lua) {
  sol::usertype<SQLiteConnection> sqlite_type = lua.new_usertype<SQLiteConnection>("SQLite", sol::no_constructor);
  sqlite_type.set_function("open", &SQLiteConnection::Open);
  sqlite_type["query"] = &SQLiteConnection::Query;
  sqlite_type["execute"] = &SQLiteConnection::Execute;
  sqlite_type["close"] = &SQLiteConnection::Close;
  sqlite_type["isOpen"] = &SQLiteConnection::IsOpen;

  sol::usertype<MySQLConnection> mysql_type = lua.new_usertype<MySQLConnection>("MySQL", sol::no_constructor);
  mysql_type.set_function("open", &MySQLConnection::Open);
  mysql_type["query"] = &MySQLConnection::Query;
  mysql_type["execute"] = &MySQLConnection::Execute;
  mysql_type["close"] = &MySQLConnection::Close;
  mysql_type["isOpen"] = &MySQLConnection::IsOpen;
}

}  // namespace lua::bindings
