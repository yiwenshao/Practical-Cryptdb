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

//representation of one field.
struct FieldMeta_Wrapper{
    bool hasSalt;
    FieldMeta *originalFm;
    std::vector<int> choosenOnions;
    std::vector<onion> choosen_onions_o;
    std::vector<std::string> choosen_onions_str;
    //used to construct return meta
    int onionIndex = 0;
    int numOfOnions=0;
    //onions
    std::vector<std::string> fields;
    std::vector<onion> onions;
    std::vector<std::string> onion_str;
    std::vector<OnionMeta*>originalOm;
    void show();
};


/*Transformed version of FieldMeta*/
class FieldMetaTrans{
    FieldMeta *originalFm;
    bool hasSalt;
    std::string saltName;

    std::vector<OnionMeta*> onionsOm;
    std::vector<onion> onionsO;
    std::vector<std::string> onionsName;

    std::vector<int> choosenIndex;
    std::vector<onion> choosenOnionO;
    std::vector<std::string> choosenOnionName;
public:
    void trans(FieldMeta *fm); 
    void choose(std::vector<onion> onionSet);
    void choose(std::vector<int> onionIndexSet);

    const std::vector<std::string> getChoosenOnionName(){return choosenOnionName;}
    bool getHasSalt(){return hasSalt;}
    std::string getSaltName(){return saltName;}
};

/*Functions*/
Item_null*
make_null(const std::string &name="");
std::vector<Item *>
itemNullVector(unsigned int count);

ResType rawMySQLReturnValue_to_ResType(bool isNULL,rawMySQLReturnValue *inRow = NULL,int in_last_insert_id = 0);


void
addToReturn(ReturnMeta *const rm, int pos, const OLK &constr, bool has_salt, const std::string &name);
void
addSaltToReturn(ReturnMeta *const rm, int pos);

std::vector<FieldMeta *> getFieldMeta(SchemaInfo &schema,
                                      std::string db = "tdb",
                                      std::string table="student1");
std::unique_ptr<SchemaInfo> myLoadSchemaInfo(std::string embeddedDir="shadow");

Item *
decrypt_item_layers(const Item &i, const FieldMeta *const fm, onion o,
                    uint64_t IV);

ResType decryptResults(const ResType &dbres, const ReturnMeta &rmeta);

std::vector<FieldMeta_Wrapper> FieldMeta_to_Wrapper(std::vector<FieldMeta *> pfms);

void transform_to_rawMySQLReturnValue(rawMySQLReturnValue & str,ResType & item);

rawMySQLReturnValue
executeAndGetResultRemote(Connect * curConn,std::string query);


void
write_row_data(rawMySQLReturnValue& resraw,std::string db,std::string table,std::string prefix="data/");


