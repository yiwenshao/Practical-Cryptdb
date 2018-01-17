#pragma once
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
class WrapperState {
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);
    KillZone kill_zone;
public:
    std::string last_query;
    std::string default_db;
    WrapperState() {}
    ~WrapperState() {}
    const std::unique_ptr<QueryRewrite> &getQueryRewrite() const {
        assert(this->qr);
        return this->qr;
    }
    void setQueryRewrite(std::unique_ptr<QueryRewrite> &&in_qr) {
        this->qr = std::move(in_qr);
    }
    void selfKill(KillZone::Where where) {
        kill_zone.die(where);
    }
    void setKillZone(const KillZone &kz) {
        kill_zone = kz;
    }
    std::unique_ptr<ProxyState> ps;
    std::vector<SchemaInfoRef> schema_info_refs;
private:
    std::unique_ptr<QueryRewrite> qr;
};

/*Raw return value from mysql*/
struct rawMySQLReturnValue {
    std::vector<std::vector<std::string>> rowValues;/*data tuples*/
    std::vector<std::string> fieldNames;
    std::vector<enum_field_types> fieldTypes;
    std::vector<int> lengths;
    std::vector<int> maxlengths;/*what's the difference between length and maxlength?*/
    std::vector<int> choosen_onions;
    void show();
};

//Item_null*
//make_null(const std::string &name);

//std::vector<Item *>
//itemNullVector(unsigned int count);

//ResType MygetResTypeFromLuaTable(bool isNULL,rawMySQLReturnValue *inRow = NULL,int in_last_insert_id = 0);


