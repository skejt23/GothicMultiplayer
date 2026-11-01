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

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "Script.h"

namespace {

class DatabaseBindingTest : public ::testing::Test {
protected:
  void SetUp() override {
    test_directory_ = std::filesystem::current_path() / "data" / "internal" / "database-binding-test";
    std::error_code error;
    std::filesystem::remove_all(test_directory_, error);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(test_directory_, error);
  }

  std::filesystem::path test_directory_;
};

TEST_F(DatabaseBindingTest, JSONReportsPersistenceErrorsAndUsesOneBasedKeys) {
  LuaScript script;
  auto& lua = script.GetLuaState();
  const auto opened = lua.safe_script(R"lua(
    assert(JsonStore == nil)
    store = assert(JSON.open('database-binding-test/store.json'))
    local ok, error = store:set('first', 1)
    assert(ok and error == nil)
    assert(store:set('second', 2))
    assert(store:count() == 2)
    assert(store:keyAt(0) == nil and store:keyAt(1) == 'first')
    assert(store:keyAt(2) == 'second' and store:keyAt(3) == nil)
    local reloaded = assert(JSON.open('database-binding-test/store.json'))
    assert(reloaded:get('first') == 1 and reloaded:get('second') == 2)
  )lua",
                                      sol::script_pass_on_error);
  ASSERT_TRUE(opened.valid());
  // A directory at the file path makes writes fail on Windows and POSIX alike.
  const auto file = test_directory_ / "store.json";
  ASSERT_TRUE(std::filesystem::remove(file));
  ASSERT_TRUE(std::filesystem::create_directory(file));
  const auto failed = lua.safe_script(R"lua(
    local ok, error = store:set('third', 3)
    assert(ok == false and type(error.message) == 'string')
    assert(store:get('third') == 3)
    ok, error = store:remove('first')
    assert(ok == false and type(error.message) == 'string')
    ok, error = store:clear()
    assert(ok == false and type(error.message) == 'string')
    local missing, openError = JSON.open('../outside.json')
    assert(missing == nil and type(openError.message) == 'string')
  )lua",
                                      sol::script_pass_on_error);
  if (!failed.valid()) {
    const sol::error error = failed;
    FAIL() << error.what();
  }
}

TEST_F(DatabaseBindingTest, SQLiteUsesPreparedParametersAndTypedResults) {
  LuaScript script;
  sol::state& lua = script.GetLuaState();

  sol::protected_function_result result = lua.safe_script(R"lua(
    local db, openError = SQLite.open({
      database = "database-binding-test/nested/server.db",
      busyTimeout = 100
    })
    assert(db ~= nil, openError and openError.message)
    assert(db:isOpen())

    local createResult, createError = db:execute([[
      CREATE TABLE users (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        score REAL NOT NULL,
        enabled INTEGER NOT NULL,
        note TEXT NULL,
        payload BLOB NOT NULL
      ); -- trailing comments are allowed
    ]])
    assert(createResult ~= nil, createError and createError.message)
    assert(#createResult.rows == 0)

    local insertResult, insertError = db:execute(
      "INSERT INTO users (id, name, score, enabled, note, payload) VALUES (?, ?, ?, ?, ?, ?)",
      42, "O'Reilly", 12.5, true, nil, "a\0b"
    )
    assert(insertResult ~= nil, insertError and insertError.message)
    assert(insertResult.affectedRows == 1)
    assert(insertResult.lastInsertId == 42)

    local selectResult, selectError = db:query(
      "SELECT id, name, score, enabled, note, payload FROM users WHERE id = ?",
      42
    )
    assert(selectResult ~= nil, selectError and selectError.message)
    assert(#selectResult.rows == 1)
    assert(#selectResult.columns == 6)
    assert(selectResult.rows[1].id == 42)
    assert(selectResult.rows[1].name == "O'Reilly")
    assert(selectResult.rows[1].score == 12.5)
    assert(selectResult.rows[1].enabled == 1)
    assert(selectResult.rows[1].note == nil)
    assert(selectResult.rows[1].payload == "a\0b")

    local nullResult, nullError = db:query("SELECT ? AS value", nil)
    assert(nullResult ~= nil, nullError and nullError.message)
    assert(nullResult.rows[1].value == nil)

    local badResult, badError = db:query("SELECT ?")
    assert(badResult == nil)
    assert(badError.driver == "sqlite")
    assert(type(badError.message) == "string")

    assert(db:close())
    assert(not db:isOpen())
    local closedResult, closedError = db:query("SELECT 1")
    assert(closedResult == nil)
    assert(closedError.message == "SQLite connection is closed")

    local escaped, escapedError = SQLite.open({ database = "../server.db" })
    assert(escaped == nil)
    assert(escapedError.driver == "sqlite")
    assert(escapedError.message == "Invalid SQLite database path")
  )lua",
                                                          sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST_F(DatabaseBindingTest, MySQLValidatesOptionsBeforeConnecting) {
  LuaScript script;
  sol::state& lua = script.GetLuaState();

  sol::protected_function_result result = lua.safe_script(R"lua(
    local db, openError = MySQL.open({})
    assert(db == nil)
    assert(openError.driver == "mysql")
    assert(openError.code == 0)
    assert(openError.message == "Missing required 'database' option")
  )lua",
                                                          sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
