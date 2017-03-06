#include <string>

#include <main/stored_procedures.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>

std::vector<std::string>
getStoredProcedures()
{
    const std::vector<std::string> &add_procs{
        " CREATE PROCEDURE " + MetaData::Proc::activeTransactionP() + "()\n"
        " BEGIN\n"
        "   DECLARE eat_result BIGINT;\n"
        "   DECLARE trx_count BIGINT;\n\n"

            // force transaction propagation
        "   SELECT 1 FROM remote_db.generic_prefix_remoteQueryCompletion\n"
        "    LIMIT 1\n"
        "     INTO eat_result;\n\n"

        "   SELECT COUNT(trx_id) INTO trx_count\n"
        "     FROM INFORMATION_SCHEMA.INNODB_TRX\n"
        "    WHERE INFORMATION_SCHEMA.INNODB_TRX.TRX_MYSQL_THREAD_ID =\n"
        "          (SELECT CONNECTION_ID())\n"
        "      AND INFORMATION_SCHEMA.INNODB_TRX.TRX_STATE =\n"
        "          'RUNNING';\n\n"

        "   SELECT NOT 0 = trx_count AS ACTIVE_TRX;\n"
        " END\n"};

    return add_procs;
}

static bool
addStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    auto procs = getStoredProcedures();
    for (auto it : procs) {
        RETURN_FALSE_IF_FALSE(conn->execute(it));
    }

    return true;
}

static bool
dropStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    const std::vector<std::string>
        drop_procs{MetaData::Proc::activeTransactionP()};

    for (auto it : drop_procs) {
        RETURN_FALSE_IF_FALSE(conn->execute("DROP PROCEDURE IF EXISTS " + it));
    }

    return true;
}

bool
loadStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    RETURN_FALSE_IF_FALSE(dropStoredProcedures(conn));
    RETURN_FALSE_IF_FALSE(addStoredProcedures(conn));

    return true;
}

