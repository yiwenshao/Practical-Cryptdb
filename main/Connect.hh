#pragma once

/*
 * Connect.h
 *
 */

#include <vector>
#include <string>
#include <memory>

#include <util/util.hh>
#include <parser/sql_utils.hh>

#include <mysql.h>
typedef MYSQL_RES DBResult_native;

extern "C" void *create_embedded_thd(int client_flag);

class DBResult {
 public:
    DBResult(DBResult_native *const n, bool success,
             uint64_t affected_rows, uint64_t insert_id)
        : n(n), success(success), affected_rows(affected_rows),
          insert_id(insert_id) {}

    ~DBResult();
    DBResult_native *const n;

    //returns data from this db result
    ResType unpack();

    static DBResult *store(MYSQL *const mysql);

    bool getSuccess() const {return success;}

 private:
    const bool success;
    const uint64_t affected_rows;
    const uint64_t insert_id;
};

class Connect {
 public:
    Connect(const std::string &server, const std::string &user,
            const std::string &passwd, uint port = 0);

    Connect(MYSQL *const _conn) : conn(_conn), close_on_destroy(true) { }

    //returns Connect for the embedded server
    static Connect *getEmbedded(const std::string &embed_dir);

    // returns true if execution was ok; caller must delete DBResult
    bool execute(const std::string &query, std::unique_ptr<DBResult> *res,
                 bool multiple_resultsets=false);
    bool execute(const std::string &query, bool multiple_resultsets=false);

    // returns error message if a query caused error
    std::string getError();

    my_ulonglong last_insert_id();

    unsigned long long get_thread_id();
    unsigned long real_escape_string(char *const to,
                                     const char *const from,
                                     unsigned long length);
    unsigned int get_mysql_errno();

    uint64_t get_affected_rows();

    ~Connect();

 private:
    MYSQL *conn;

    void do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port);

    bool close_on_destroy;
};

bool strictMode(Connect *const c);

