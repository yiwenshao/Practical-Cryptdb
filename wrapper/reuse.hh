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

/*Raw return value from mysql*/
struct MySQLColumnData {
    std::vector<std::vector<std::string>> columnData;/*data tuples*/
    std::vector<std::string> fieldNames;
    std::vector<enum_field_types> fieldTypes;
    std::vector<int> maxLengths;/*what's the difference between length and maxlength?*/
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
    int saltType;
    int saltLength;

    std::vector<OnionMeta*> onionsOm;
    std::vector<onion> onionsO;
    std::vector<std::string> onionsName;

    std::vector<int> choosenIndex;
    std::vector<onion> choosenOnionO;
    std::vector<std::string> choosenOnionName;

    std::vector<int> choosenFieldTypes;
    std::vector<int> choosenFieldLengths;
public:
    FieldMeta *getOriginalFieldMeta(){return originalFm;}
    void showChoosenOnionO(){std::cout<<"for field "<<originalFm->getFieldName()<<std::endl;
        for(auto item:choosenOnionO){
            std::cout<<TypeText<onion>::toText(item)<<", ";
        }
        std::cout<<std::endl;
    }
    void trans(FieldMeta *fm); 
    void choose(std::vector<onion> onionSet);
    void choose(std::vector<int> onionIndexSet);
    void chooseAll(){choosenOnionO = onionsO; choosenOnionName = onionsName;}
    const std::vector<std::string> getChoosenOnionName(){return choosenOnionName;}
    void setChoosenOnionName(const std::vector<std::string> input){choosenOnionName=input;}

    const std::vector<onion> getChoosenOnionO(){return choosenOnionO;}
    void setChoosenOnionO(std::vector<onion> input){choosenOnionO=input;}

    bool getHasSalt(){return hasSalt;}
    void setHasSalt(bool input){hasSalt=input;}

    std::string getSaltName(){return saltName;}
    void setSaltName(std::string input){saltName=input;}

    std::vector<int> getChoosenFieldTypes(){return choosenFieldTypes;}
    void setChoosenFieldTypes(std::vector<int> input){choosenFieldTypes = input;}

    std::vector<int> getChoosenFieldLengths(){return choosenFieldLengths;}
    void setChoosenFieldLengths(std::vector<int> input){choosenFieldLengths=input;}

    std::string getFieldPlainName(){return originalFm->getFieldName(); }

    int getSaltType(){return saltType;}
    void setSaltType(int input){saltType=input;}

    int getSaltLength(){return saltLength;}
    void setSaltLength(int input){saltLength=input;}
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

MySQLColumnData
executeAndGetColumnData(Connect * conn,std::string query);



rawMySQLReturnValue 
executeAndGetResultRemoteWithOneVariableLen(Connect * curConn,
                                           std::string query,
                                           std::vector<int> &vlen,
                                           std::vector<std::string> &vstr,
std::string &vname);

void
write_row_data(rawMySQLReturnValue& resraw,std::string db,std::string table,std::string prefix="data/");

void storeStrategies(std::vector<FieldMetaTrans>& res);

enum class STORE_STRATEGY{
    FIRST,
    ALL
};

int getDecryptionOnionIndex(FieldMetaTrans& fdtrans);


struct fullBackUp{
    std::vector<std::string> field_names;//can either be anno onion name or field name
    std::vector<int> field_types;
    std::vector<int> field_lengths;
    std::map<std::string,std::vector<std::string>> annoOnionNameToFileVector;//field name to vector of string
    std::map<std::string,std::vector<Item*>> annoOnionNameToItemVector;
    std::map<std::string,int> annoOnionNameToType;
};


void load_num_file(std::string filename,std::vector<std::string> &res);
void load_string_file(std::string filename, std::vector<std::string> &res,unsigned long length);

std::ostream&
insertManyValues(std::ostream &out,List<List_item> &newList);


std::unique_ptr<Item>
getIntItem(int i);

std::unique_ptr<Item>
getStringItem(std::string s);


void 
load_num_file_count(std::string filename,
              std::vector<std::string> &res,
	      int count);

void
load_string_file_count(std::string filename, 
                       std::vector<std::string> &res,
                       unsigned long length,
                       int count);


void 
loadFileEscape(std::string filename,
                    std::vector<std::string> &res,
                    unsigned int maxLength);

void 
loadFileNoEscape(std::string filename,            
              std::vector<std::string> &res);


void
writeRowdataEscapeString(const std::vector<std::string> &column,
                      std::string columnFilename,
                      unsigned int maxLength);


void 
writeRowdataNum(const std::vector<std::string> &column,
                      std::string columnFilename);


void loadFileEscapeLimitCount(std::string filename,
                    std::vector<std::string> &res,
                    unsigned int maxLength,int limit);

void 
loadFileNoEscapeLimitCount(std::string filename,
                 std::vector<std::string> &res,int limit);


//Connect * initEmbeddedAndRemoteConnection(std::string ip,int port);

