#include <ostream>

#include <parser/sql_utils.hh>
#include <parser/lex_util.hh>
#include <parser/stringify.hh>
#include <mysql.h>



using namespace std;

static bool lib_initialized = false;

void
init_mysql(const string &embed_db)
{
    // FIXME: can still get a weird case where something calls
    // init_mysql(...) and lib_initialized is true so it continues on to
    // execute a query against the embedded database; but the thread
    // initializing the embedded database still hasn't completed
    if (!__sync_bool_compare_and_swap(&lib_initialized, false, true)) {
        return;
    }

    char dir_arg[1024];
    snprintf(dir_arg, sizeof(dir_arg), "--datadir=%s", embed_db.c_str());

    const char *mysql_av[] =
    { "progname",
            "--skip-grant-tables",
            dir_arg,
            /* "--skip-innodb", */
            /* "--default-storage-engine=MEMORY", */
            "--character-set-server=utf8",
            "--language=" MYSQL_BUILD_DIR "/sql/share/"
    };

    assert(0 == mysql_library_init(sizeof(mysql_av)/sizeof(mysql_av[0]),
                                  (char**) mysql_av, 0));
    assert(0 == mysql_thread_init());
}

char *
make_thd_string(const string &s, size_t *lenp)
{
    THD *thd = current_thd;
    assert(thd);
    if (lenp)
        *lenp = s.size();
    return thd->strmake(s.data(), s.size());
}

string
ItemToString(const Item &i) {
    if (RiboldMYSQL::is_null(i)) {
        return std::string("NULL");
    }

    bool is_null;
    const std::string &s0 = RiboldMYSQL::val_str(i, &is_null);
    assert(false == is_null);

    return s0;
}

std::string
printItemToString(const Item &i)
{
    std::ostringstream o;
    o << i;
    return o.str();
}

