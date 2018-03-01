#include <rpl_reporting.h>
#include <rpl_rli.h>
#include <sql_base.h>

#define UNUSED(x) (void)(x)
void __attribute__((noreturn))
Slave_reporting_capability::report(loglevel, int, char const*, ...) const
{
    abort();
}

void __attribute__((noreturn))
Relay_log_info::slave_close_thread_tables(THD*)
{
    abort();
}

extern "C" void _ZN14Relay_log_infoC1Eb() __attribute__((noreturn));

void
_ZN14Relay_log_infoC1Eb()
{
    abort();
}

static
void
do_create_table() {
    UNUSED(current_thd);
}

static inline
void
token(){
    UNUSED(do_create_table);
}

