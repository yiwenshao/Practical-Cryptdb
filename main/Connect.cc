/*
 * Connect.cpp
 *
 *  Created on: Dec 1, 2010
 *      Author: raluca
 */

#include <stdexcept>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>

#include <util/cryptdb_log.hh>
#include <main/Connect.hh>
#include <main/macro_util.hh>
#include <main/Analysis.hh>
#include <parser/mysql_type_metadata.hh>

__thread ProxyState *thread_ps = NULL;

Connect::Connect(const std::string &server, const std::string &user,
                 const std::string &passwd, uint port)
    : conn(nullptr), close_on_destroy(true)
{
    do_connect(server, user, passwd, port);
}

bool
strictMode(Connect *const c)
{
    return c->execute("SET SESSION sql_mode = 'ANSI,TRADITIONAL'");
}

void
Connect::do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port)
{
    const char *dummy_argv[] = {
        "progname",
        "--skip-grant-tables",
        "--skip-innodb",
        "--default-storage-engine=MEMORY",
        "--character-set-server=utf8",
        "--language=" MYSQL_BUILD_DIR "/sql/share/"
    };
    assert(0 == mysql_library_init(sizeof(dummy_argv)/sizeof(*dummy_argv),
                                   const_cast<char**>(dummy_argv), 0));

    conn = mysql_init(NULL);

    /* Connect via TCP, and not via Unix domain sockets */
    const uint proto = MYSQL_PROTOCOL_TCP;
    mysql_options(conn, MYSQL_OPT_PROTOCOL, &proto);

    /* Connect to real server even if linked against embedded libmysqld */
    mysql_options(conn, MYSQL_OPT_USE_REMOTE_CONNECTION, 0);

    {
        my_bool reconnect = 1;
        /* automatically reconnect */
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
    }

    /* Connect to database */
    if (!mysql_real_connect(conn, server.c_str(), user.c_str(),
                            passwd.c_str(), 0, port, 0,
                            CLIENT_MULTI_STATEMENTS)) {
        LOG(warn) << "connecting to server " << server
                  << " user " << user
                  << " pwd " << passwd
                  << " port " << port;
        LOG(warn) << "mysql_real_connect: " << mysql_error(conn);
        throw std::runtime_error("cannot connect");
    }
}

Connect *Connect::getEmbedded(const std::string &embed_db)
{
    init_mysql(embed_db);

    MYSQL *const m = mysql_init(0);
    assert(m);

    mysql_options(m, MYSQL_OPT_USE_EMBEDDED_CONNECTION, 0);

    if (!mysql_real_connect(m, 0, 0, 0, 0, 0, 0,
                            CLIENT_MULTI_STATEMENTS)) {
        mysql_close(m);
        thrower() << "mysql_real_connect: " << mysql_error(m);
    }

    return new Connect(m);
}

// @multiple_resultsets causes us to ignore query results.
// > This is a hack that allows us to deal with potentially multiple
//   sets returned when CALLing a stored procedure.
bool
Connect::execute(const std::string &query, std::unique_ptr<DBResult> *res,
                 bool multiple_resultsets)
{
    //silently ignore empty queries
    if (query.length() == 0) {
        LOG(warn) << "empty query";
        *res = nullptr;
        return true;
    }
    bool success = true;
    if (mysql_query(conn, query.c_str())) {
//        LOG(warn) << "mysql_query: " << mysql_error(conn);
        LOG(warn) << "error on query: " << query;
        *res = nullptr;
        success = false;
    } else {
        if (false == multiple_resultsets) {
            *res = std::unique_ptr<DBResult>(DBResult::store(conn));
        } else {
            // iterate through each result set; if a query leading to
            // one of the resultsets failed, it will be the last resultset,
            // so get the error value
            const bool errno_success = 0 == mysql_errno(conn);
            while (true) {
                DBResult_native *const res_native =
                    mysql_store_result(conn);

                const int status = mysql_next_result(conn);
                if (0 == status) {                  // another result
                    if (res_native) {
                        mysql_free_result(res_native);
                    }
                } else if (-1 == status) {          // last result
                    *res = std::unique_ptr<DBResult>(
                        new DBResult(res_native, errno_success,
                                     mysql_affected_rows(conn),
                                     mysql_insert_id(conn)));
                    break;
                } else {                            // error
                    thrower() << "error occurred processing multiple"
                                 " query results";
                }
            }

            *res = nullptr;
        }
    }

    if (thread_ps) {
        thread_ps->safeCreateEmbeddedTHD();
    } else {
        assert(create_embedded_thd(0));
    }

    return success;
}


// because the caller is ignoring the ResType we must account for
// errors encoded in the ResType
bool
Connect::execute(const std::string &query, bool multiple_resultsets)
{
    std::unique_ptr<DBResult> aux;
    const bool r = execute(query, &aux, multiple_resultsets);
    return r && aux->getSuccess();
}

std::string
Connect::getError()
{
    return mysql_error(conn);
}

my_ulonglong
Connect::last_insert_id()
{
    return mysql_insert_id(conn);
}

unsigned long long 
Connect::get_thread_id(){
    if(conn!=NULL)
        return mysql_thread_id(conn);
    else{
        std::cout<<"no connection, no id"<<std::endl;
        return -1;
    }
}



unsigned long
Connect::real_escape_string(char *const to, const char *const from,
                            unsigned long length)
{
    return mysql_real_escape_string(conn, to, from, length);
}

unsigned int
Connect::get_mysql_errno()
{
    return mysql_errno(conn);
}

uint64_t 
Connect::get_affected_rows(){
    return mysql_affected_rows(conn);
}


Connect::~Connect()
{
    if (close_on_destroy) {
        mysql_close(conn);
    }
}

DBResult *
DBResult::store(MYSQL *const mysql)
{
    const bool success = 0 == mysql_errno(mysql);
    DBResult_native *const n = mysql_store_result(mysql);
    return new DBResult(n, success, mysql_affected_rows(mysql),
                        mysql_insert_id(mysql));
}

DBResult::~DBResult()
{
    mysql_free_result(n);
}

static Item *
getItem(char *const content, enum_field_types type, uint len)
{
    if (content == NULL) {
        return new Item_null();
    }
    const std::string content_str = std::string(content, len);
    if (isMySQLTypeNumeric(type)) {
        const ulonglong val = valFromStr(content_str);
        return new Item_int(val);
    } else {
        return new Item_string(make_thd_string(content_str), len,
                               &my_charset_bin);
    }
}

// > returns the data in the last server response
// > TODO: to optimize return pointer to avoid overcopying large
//   result sets?
// > This function must not return pointers (internal or otherwise) to
//   objects that it owns.
//   > ie, We must be able to delete 'this' without mangling the 'ResType'
//     returned from this->unpack(...).
ResType
DBResult::unpack()
{
    // 'n' will be NULL when the mysql statement doesn't return a resultset
    // > ie INSERT
    if (nullptr == n) {
        return ResType(this->success, this->affected_rows, this->insert_id);
    }

    const size_t row_count = static_cast<size_t>(mysql_num_rows(n));
    const int col_count    = mysql_num_fields(n);

    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    for (int j = 0;; j++) {
        MYSQL_FIELD *const field = mysql_fetch_field(n);
        if (!field) {
            assert(col_count == j);
            break;
        }

        names.push_back(field->name);
        types.push_back(field->type);
    }

    std::vector<std::vector<Item *> > rows;
    for (size_t index = 0;; index++) {
        const MYSQL_ROW row = mysql_fetch_row(n);
        if (!row) {
            assert(row_count == index);
            break;
        }
        unsigned long *const lengths = mysql_fetch_lengths(n);

        std::vector<Item *> resrow;

        for (int j = 0; j < col_count; j++) {
            Item *const item = getItem(row[j], types[j], lengths[j]);
            resrow.push_back(item);
        }

        rows.push_back(resrow);
    }

    return ResType(this->success, this->affected_rows, this->insert_id,
                   std::move(names), std::move(types), std::move(rows));
}
