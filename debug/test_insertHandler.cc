#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>

static std::string embeddedDir="/t/cryt/shadow";
/*
static std::string getInsertResults(Analysis a,LEX* lex){
        LEX *const new_lex = copyWithTHD(lex);
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        const std::string &db_name =
            lex->select_lex.table_list.first->db;
        TEST_DatabaseDiscrepancy(db_name, a.getDatabaseName());
        //from databasemeta to tablemeta.
        const TableMeta &tm = a.getTableMeta(db_name, table);

        //rewrite table name
        new_lex->select_lex.table_list.first =
            rewrite_table_list(lex->select_lex.table_list.first, a);

        // -------------------------
        // Fields (and default data)
        // -------------------------
        // If the fields list doesn't exist, or is empty; the 'else' of
        // this code block will put every field into fmVec.
        // > INSERT INTO t VALUES (1, 2, 3);
        // > INSERT INTO t () VALUES ();
        // FIXME: Make vector of references.
        std::vector<FieldMeta *> fmVec;
        std::vector<Item *> implicit_defaults;

        if (lex->field_list.head()) {
            auto it = List_iterator<Item>(lex->field_list);
            List<Item> newList;
            for (;;) {
                const Item *const i = it++;
                if (!i) {
                    break;
                }
                //这下也就知道了field item是什么了
                TEST_TextMessageError(i->type() == Item::FIELD_ITEM,
                                      "Expected field item!");
                const Item_field *const ifd =
                    static_cast<const Item_field *>(i);
                FieldMeta &fm =
                    a.getFieldMeta(db_name, ifd->table_name,
                                   ifd->field_name);
                fmVec.push_back(&fm);
                rewriteInsertHelper(*i, fm, a, &newList);
            }

            // Collect the implicit defaults.
            // > Such must be done because fields that can not have NULL
            // will be implicitly converted by mysql sans encryption.
            std::vector<FieldMeta *> field_implicit_defaults =
                vectorDifference(tm.defaultedFieldMetas(), fmVec);
            const Item_field *const seed_item_field =
                static_cast<Item_field *>(new_lex->field_list.head());
            for (auto implicit_it : field_implicit_defaults) {
                // Get default fields.
                const Item_field *const item_field =
                    make_item_field(*seed_item_field, table,
                                    implicit_it->getFieldName());
                rewriteInsertHelper(*item_field, *implicit_it, a,
                                    &newList);

                // Get default values.
                const std::string def_value = implicit_it->defaultValue();
                rewriteInsertHelper(*make_item_string(def_value),
                                    *implicit_it, a, &implicit_defaults);
            }

            new_lex->field_list = newList;
         } else {
            // No field list, use the table order.
            // > Because there is no field list, fields with default
            //   values must be explicity INSERTed so we don't have to
            //   take any action with respect to defaults.
            assert(fmVec.empty());
            std::vector<FieldMeta *> fmetas = tm.orderedFieldMetas();
            fmVec.assign(fmetas.begin(), fmetas.end());
        }


        if (lex->many_values.head()) {
            //start processing many values
            auto it = List_iterator<List_item>(lex->many_values);
            List<List_item> newList;
            for (;;) {
                List_item *const li = it++;
                if (!li) {
                    break;
                }
                List<Item> *const newList0 = new List<Item>();
                if (li->elements != fmVec.size()) {
                    TEST_TextMessageError(0 == li->elements
                                         && NULL == lex->field_list.head(),
                                          "size mismatch between fields"
                                          " and values!");
                    // Query such as this.
                    // > INSERT INTO <table> () VALUES ();
                    // > INSERT INTO <table> VALUES ();
                } else {
                    auto it0 = List_iterator<Item>(*li);
                    auto fmVecIt = fmVec.begin();

                    for (;;) {
                        const Item *const i = it0++;
                        assert(!!i == (fmVec.end() != fmVecIt));
                        if (!i) {
                            break;
                        }
                        //fetch values, and use fieldMeta to facilitate rewrite
                        //every filed should be encrypted with onions of encryption
                        rewriteInsertHelper(*i, **fmVecIt, a, newList0);
                        ++fmVecIt;
                    }
                    for (auto def_it : implicit_defaults) {
                        newList0->push_back(def_it);
                    }
                }
                newList.push_back(newList0);
            }
            new_lex->many_values = newList;
        }
        {
            auto fd_it = List_iterator<Item>(lex->update_list);
            auto val_it = List_iterator<Item>(lex->value_list);
            List<Item> res_fields, res_values;
            TEST_TextMessageError(
                rewrite_field_value_pairs(fd_it, val_it, a, &res_fields,
                                          &res_values),
                "rewrite_field_value_pairs failed in ON DUPLICATE KEY"
                " UPDATE");
            new_lex->update_list = res_fields;
            new_lex->value_list = res_values;
        }
        return lexToQuery(*lex);
}*/


static void testInsertHandler(std::string query){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());
    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };
    //load all metadata and then store it in schema
    loadChildren(schema.get()); 
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    //just like what we do in Rewrite::rewrite,dispatchOnLex
    Analysis analysis(std::string("tdb"),*schema,TK,
                        SECURITY_RATING::SENSITIVE);
    DMLHandler *h = new InsertHandler();
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse("tdb", query));
    LEX *const lex = p->lex();
    auto executor = h->transformLex(analysis,lex);
    std::cout<<((DMLQueryExecutor*)executor)->getQuery()<<std::endl;
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string query1 = "insert into student values(1,\"zhangfei\")";
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testInsertHandler(item);
        std::cout<<std::endl;
    }
    return 0;
}
