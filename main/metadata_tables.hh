#pragma once

#include <string>
#include <memory>

#include <main/Connect.hh>

//这个在embedded的表中也是定义好的query最大的长度, 在程序中检查不能超过这个长度.
#define STORED_QUERY_LENGTH 10000

namespace MetaData {
    bool initialize(const std::unique_ptr<Connect> &conn,
                    const std::unique_ptr<Connect> &e_conn,
                    const std::string &prefix);

    namespace Table {
        std::string metaObject();
        std::string bleedingMetaObject();
        std::string embeddedQueryCompletion();
        std::string staleness();
        std::string showDirective();
        std::string remoteQueryCompletion();
    };

    namespace Proc {
        std::string activeTransactionP();
    };

    namespace DB {
        std::string embeddedDB();
        std::string remoteDB();
    };

    namespace Internal {
        void initPrefix(const std::string &prefix);
        const std::string &getPrefix();
        const std::string &lowLevelPrefix(const char *const p);
    };
};

