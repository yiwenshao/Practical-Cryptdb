#include "showHelper/show_utilities.hh"



namespace show{
    void draw(std::string s,color c){
        if(s.size()==0){
            std::cout<<"0string"<<std::endl;
        }else{
            switch(c){
                case GREEN:
                    std::cout<<GREEN_BEGIN<<s<<COLOR_END<<std::endl;
                    break;
                case BLANK:
                    std::cout<<s<<std::endl;
                    break;
                case RED:
                    std::cout<<RED_BEGIN<<s<<COLOR_END<<std::endl;
                    break;
                case BOLD:
                    std::cout<<BOLD_BEGIN<<s<<COLOR_END<<std::endl;
                    break;
                default:
                    ;
            }
        }
    }
    namespace keytrans{
        std::map<int,std::string> trans{
            {Key::PRIMARY,"PRIMARY"},
            {Key::UNIQUE,"UNIQUE"},
            {Key::MULTIPLE,"MULTIPLE"},
            {Key::FULLTEXT,"FULLTEXT"},
            {Key::FOREIGN_KEY,"FOREIGN_KEY"},
            {Key::SPATIAL,"SPATIAL"}
        };
    }
    namespace commandtrans{
        std::map<int,std::string> trans{
            {SQLCOM_SELECT, "SQLCOM_SELECT" },
            {SQLCOM_CREATE_TABLE, "SQLCOM_CREATE_TABLE" },
            {SQLCOM_CREATE_INDEX, "SQLCOM_CREATE_INDEX" },
            {SQLCOM_ALTER_TABLE, "SQLCOM_ALTER_TABLE" },
            {SQLCOM_UPDATE, "SQLCOM_UPDATE" },
            {SQLCOM_INSERT, "SQLCOM_INSERT" },
            {SQLCOM_INSERT_SELECT, "SQLCOM_INSERT_SELECT" },
            {SQLCOM_DELETE, "SQLCOM_DELETE" },
            {SQLCOM_TRUNCATE, "SQLCOM_TRUNCATE" },
            {SQLCOM_DROP_TABLE, "SQLCOM_DROP_TABLE" },
            {SQLCOM_DROP_INDEX, "SQLCOM_DROP_INDEX" },
            {SQLCOM_SHOW_DATABASES, "SQLCOM_SHOW_DATABASES" },
            {SQLCOM_SHOW_TABLES, "SQLCOM_SHOW_TABLES" },
            {SQLCOM_SHOW_FIELDS, "SQLCOM_SHOW_FIELDS" },
            {SQLCOM_SHOW_KEYS, "SQLCOM_SHOW_KEYS" },
            {SQLCOM_SHOW_VARIABLES, "SQLCOM_SHOW_VARIABLES" },
            {SQLCOM_SHOW_STATUS, "SQLCOM_SHOW_STATUS" },
            {SQLCOM_SHOW_ENGINE_LOGS, "SQLCOM_SHOW_ENGINE_LOGS" },
            {SQLCOM_SHOW_ENGINE_STATUS, "SQLCOM_SHOW_ENGINE_STATUS" },
            {SQLCOM_SHOW_ENGINE_MUTEX, "SQLCOM_SHOW_ENGINE_MUTEX" },
            {SQLCOM_SHOW_PROCESSLIST, "SQLCOM_SHOW_PROCESSLIST" },
            {SQLCOM_SHOW_MASTER_STAT, "SQLCOM_SHOW_MASTER_STAT" },
            {SQLCOM_SHOW_SLAVE_STAT, "SQLCOM_SHOW_SLAVE_STAT" },
            {SQLCOM_SHOW_GRANTS, "SQLCOM_SHOW_GRANTS" },
            {SQLCOM_SHOW_CREATE, "SQLCOM_SHOW_CREATE" },
            {SQLCOM_SHOW_CHARSETS, "SQLCOM_SHOW_CHARSETS" },
            {SQLCOM_SHOW_COLLATIONS, "SQLCOM_SHOW_COLLATIONS" },
            {SQLCOM_SHOW_CREATE_DB, "SQLCOM_SHOW_CREATE_DB" },
            {SQLCOM_SHOW_TABLE_STATUS, "SQLCOM_SHOW_TABLE_STATUS" },
            {SQLCOM_SHOW_TRIGGERS, "SQLCOM_SHOW_TRIGGERS" },
            {SQLCOM_LOAD, "SQLCOM_LOAD" },
            {SQLCOM_SET_OPTION, "SQLCOM_SET_OPTION" },
            {SQLCOM_LOCK_TABLES, "SQLCOM_LOCK_TABLES" },
            {SQLCOM_UNLOCK_TABLES, "SQLCOM_UNLOCK_TABLES" },
            {SQLCOM_GRANT, "SQLCOM_GRANT" },
            {SQLCOM_CHANGE_DB, "SQLCOM_CHANGE_DB" },
            {SQLCOM_CREATE_DB, "SQLCOM_CREATE_DB" },
            {SQLCOM_DROP_DB, "SQLCOM_DROP_DB" },
            {SQLCOM_ALTER_DB, "SQLCOM_ALTER_DB" },
            {SQLCOM_REPAIR, "SQLCOM_REPAIR" },
            {SQLCOM_REPLACE, "SQLCOM_REPLACE" },
            {SQLCOM_REPLACE_SELECT, "SQLCOM_REPLACE_SELECT" },
            {SQLCOM_CREATE_FUNCTION, "SQLCOM_CREATE_FUNCTION" },
            {SQLCOM_DROP_FUNCTION, "SQLCOM_DROP_FUNCTION" },
            {SQLCOM_REVOKE, "SQLCOM_REVOKE" },
            {SQLCOM_OPTIMIZE, "SQLCOM_OPTIMIZE" },
            {SQLCOM_CHECK, "SQLCOM_CHECK" },
            {SQLCOM_ASSIGN_TO_KEYCACHE, "SQLCOM_ASSIGN_TO_KEYCACHE" },
            {SQLCOM_PRELOAD_KEYS, "SQLCOM_PRELOAD_KEYS" },
            {SQLCOM_FLUSH, "SQLCOM_FLUSH" },
            {SQLCOM_KILL, "SQLCOM_KILL" },
            {SQLCOM_ANALYZE, "SQLCOM_ANALYZE" },
            {SQLCOM_ROLLBACK, "SQLCOM_ROLLBACK" },
            {SQLCOM_ROLLBACK_TO_SAVEPOINT, "SQLCOM_ROLLBACK_TO_SAVEPOINT" },
            {SQLCOM_COMMIT, "SQLCOM_COMMIT" },
            {SQLCOM_SAVEPOINT, "SQLCOM_SAVEPOINT" },
            {SQLCOM_RELEASE_SAVEPOINT, "SQLCOM_RELEASE_SAVEPOINT" },
            {SQLCOM_SLAVE_START, "SQLCOM_SLAVE_START" },
            {SQLCOM_SLAVE_STOP, "SQLCOM_SLAVE_STOP" },
            {SQLCOM_BEGIN, "SQLCOM_BEGIN" },
            {SQLCOM_CHANGE_MASTER, "SQLCOM_CHANGE_MASTER" },
            {SQLCOM_RENAME_TABLE, "SQLCOM_RENAME_TABLE" },
            {SQLCOM_RESET, "SQLCOM_RESET" },
            {SQLCOM_PURGE, "SQLCOM_PURGE" },
            {SQLCOM_PURGE_BEFORE, "SQLCOM_PURGE_BEFORE" },
            {SQLCOM_SHOW_BINLOGS, "SQLCOM_SHOW_BINLOGS" },
            {SQLCOM_SHOW_OPEN_TABLES, "SQLCOM_SHOW_OPEN_TABLES" },
            {SQLCOM_HA_OPEN, "SQLCOM_HA_OPEN" },
            {SQLCOM_HA_CLOSE, "SQLCOM_HA_CLOSE" },
            {SQLCOM_HA_READ, "SQLCOM_HA_READ" },
            {SQLCOM_SHOW_SLAVE_HOSTS, "SQLCOM_SHOW_SLAVE_HOSTS" },
            {SQLCOM_DELETE_MULTI, "SQLCOM_DELETE_MULTI" },
            {SQLCOM_UPDATE_MULTI, "SQLCOM_UPDATE_MULTI" },
            {SQLCOM_SHOW_BINLOG_EVENTS, "SQLCOM_SHOW_BINLOG_EVENTS" },
            {SQLCOM_DO, "SQLCOM_DO" },
            {SQLCOM_SHOW_WARNS, "SQLCOM_SHOW_WARNS" },
            {SQLCOM_EMPTY_QUERY, "SQLCOM_EMPTY_QUERY" },
            {SQLCOM_SHOW_ERRORS, "SQLCOM_SHOW_ERRORS" },
            {SQLCOM_SHOW_STORAGE_ENGINES, "SQLCOM_SHOW_STORAGE_ENGINES" },
            {SQLCOM_SHOW_PRIVILEGES, "SQLCOM_SHOW_PRIVILEGES" },
            {SQLCOM_HELP, "SQLCOM_HELP" },
            {SQLCOM_CREATE_USER, "SQLCOM_CREATE_USER" },
            {SQLCOM_DROP_USER, "SQLCOM_DROP_USER" },
            {SQLCOM_RENAME_USER, "SQLCOM_RENAME_USER" },
            {SQLCOM_REVOKE_ALL, "SQLCOM_REVOKE_ALL" },
            {SQLCOM_CHECKSUM, "SQLCOM_CHECKSUM" },
            {SQLCOM_CREATE_PROCEDURE, "SQLCOM_CREATE_PROCEDURE" },
            {SQLCOM_CREATE_SPFUNCTION, "SQLCOM_CREATE_SPFUNCTION" },
            {SQLCOM_CALL, "SQLCOM_CALL" },
            {SQLCOM_DROP_PROCEDURE, "SQLCOM_DROP_PROCEDURE" },
            {SQLCOM_ALTER_PROCEDURE, "SQLCOM_ALTER_PROCEDURE" },
            {SQLCOM_ALTER_FUNCTION, "SQLCOM_ALTER_FUNCTION" },
            {SQLCOM_SHOW_CREATE_PROC, "SQLCOM_SHOW_CREATE_PROC" },
            {SQLCOM_SHOW_CREATE_FUNC, "SQLCOM_SHOW_CREATE_FUNC" },
            {SQLCOM_SHOW_STATUS_PROC, "SQLCOM_SHOW_STATUS_PROC" },
            {SQLCOM_SHOW_STATUS_FUNC, "SQLCOM_SHOW_STATUS_FUNC" },
            {SQLCOM_PREPARE, "SQLCOM_PREPARE" },
            {SQLCOM_EXECUTE, "SQLCOM_EXECUTE" },
            {SQLCOM_DEALLOCATE_PREPARE, "SQLCOM_DEALLOCATE_PREPARE" },
            {SQLCOM_CREATE_VIEW, "SQLCOM_CREATE_VIEW" },
            {SQLCOM_DROP_VIEW, "SQLCOM_DROP_VIEW" },
            {SQLCOM_CREATE_TRIGGER, "SQLCOM_CREATE_TRIGGER" },
            {SQLCOM_DROP_TRIGGER, "SQLCOM_DROP_TRIGGER" },
            {SQLCOM_XA_START, "SQLCOM_XA_START" },
            {SQLCOM_XA_END, "SQLCOM_XA_END" },
            {SQLCOM_XA_PREPARE, "SQLCOM_XA_PREPARE" },
            {SQLCOM_XA_COMMIT, "SQLCOM_XA_COMMIT" },
            {SQLCOM_XA_ROLLBACK, "SQLCOM_XA_ROLLBACK" },
            {SQLCOM_XA_RECOVER, "SQLCOM_XA_RECOVER" },
            {SQLCOM_SHOW_PROC_CODE, "SQLCOM_SHOW_PROC_CODE" },
            {SQLCOM_SHOW_FUNC_CODE, "SQLCOM_SHOW_FUNC_CODE" },
            {SQLCOM_ALTER_TABLESPACE, "SQLCOM_ALTER_TABLESPACE" },
            {SQLCOM_INSTALL_PLUGIN, "SQLCOM_INSTALL_PLUGIN" },
            {SQLCOM_UNINSTALL_PLUGIN, "SQLCOM_UNINSTALL_PLUGIN" },
            {SQLCOM_SHOW_AUTHORS, "SQLCOM_SHOW_AUTHORS" },
            {SQLCOM_BINLOG_BASE64_EVENT, "SQLCOM_BINLOG_BASE64_EVENT" },
            {SQLCOM_SHOW_PLUGINS, "SQLCOM_SHOW_PLUGINS" },
            {SQLCOM_SHOW_CONTRIBUTORS, "SQLCOM_SHOW_CONTRIBUTORS" },
            {SQLCOM_CREATE_SERVER, "SQLCOM_CREATE_SERVER" },
            {SQLCOM_DROP_SERVER, "SQLCOM_DROP_SERVER" },
            {SQLCOM_ALTER_SERVER, "SQLCOM_ALTER_SERVER" },
            {SQLCOM_CREATE_EVENT, "SQLCOM_CREATE_EVENT" },
            {SQLCOM_ALTER_EVENT, "SQLCOM_ALTER_EVENT" },
            {SQLCOM_DROP_EVENT, "SQLCOM_DROP_EVENT" },
            {SQLCOM_SHOW_CREATE_EVENT, "SQLCOM_SHOW_CREATE_EVENT" },
            {SQLCOM_SHOW_EVENTS, "SQLCOM_SHOW_EVENTS" },
            {SQLCOM_SHOW_CREATE_TRIGGER, "SQLCOM_SHOW_CREATE_TRIGGER" },
            {SQLCOM_ALTER_DB_UPGRADE, "SQLCOM_ALTER_DB_UPGRADE" },
            {SQLCOM_SHOW_PROFILE, "SQLCOM_SHOW_PROFILE" },
            {SQLCOM_SHOW_PROFILES, "SQLCOM_SHOW_PROFILES" },
            {SQLCOM_SIGNAL, "SQLCOM_SIGNAL" },
            {SQLCOM_RESIGNAL, "SQLCOM_RESIGNAL" },
            {SQLCOM_SHOW_RELAYLOG_EVENTS, "SQLCOM_SHOW_RELAYLOG_EVENTS" },
            {SQLCOM_END, "SQLCOM_END" }
        };
    }
    namespace altertrans{
        std::map<long long,std::string> trans{
            {ALTER_ADD_COLUMN,"ALTER_ADD_COLUMN"},
            {ALTER_DROP_COLUMN,"ALTER_DROP_COLUMN"},
            {ALTER_CHANGE_COLUMN,"ALTER_CHANGE_COLUMN"},
            {ALTER_ADD_INDEX,"ALTER_ADD_INDEX"},
            {ALTER_DROP_INDEX,"ALTER_DROP_INDEX"},
            {ALTER_RENAME,"ALTER_RENAME"},
            {ALTER_ORDER,"ALTER_ORDER"},
            {ALTER_OPTIONS,"ALTER_OPTIONS"},
            {ALTER_CHANGE_COLUMN_DEFAULT,"ALTER_CHANGE_COLUMN_DEFAULT"},
            {ALTER_KEYS_ONOFF,"ALTER_KEYS_ONOFF"},
            {ALTER_CONVERT,"ALTER_CONVERT"},
            {ALTER_RECREATE,"ALTER_RECREATE"},
            {ALTER_ADD_PARTITION,"ALTER_ADD_PARTITION"},
            {ALTER_DROP_PARTITION,"ALTER_DROP_PARTITION"},
            {ALTER_COALESCE_PARTITION,"ALTER_COALESCE_PARTITION"},
            {ALTER_REORGANIZE_PARTITION,"ALTER_REORGANIZE_PARTITION"},
            {ALTER_PARTITION,"ALTER_PARTITION"},
            {ALTER_ADMIN_PARTITION,"ALTER_ADMIN_PARTITION"},
            {ALTER_TABLE_REORG,"ALTER_TABLE_REORG"},
            {ALTER_REBUILD_PARTITION,"ALTER_REBUILD_PARTITION"},
            {ALTER_ALL_PARTITION,"ALTER_ALL_PARTITION"},
            {ALTER_REMOVE_PARTITIONING,"ALTER_REMOVE_PARTITIONING"},
            {ALTER_FOREIGN_KEY,"ALTER_FOREIGN_KEY"},
            {ALTER_TRUNCATE_PARTITION,"ALTER_TRUNCATE_PARTITION"}            
        };
    }

    namespace itemtypetrans{
        std::map<int,std::string> trans{
            {Item::FIELD_ITEM ,"FIELD_ITEM"},
            {Item::FUNC_ITEM ,"FUNC_ITEM"},
            {Item::SUM_FUNC_ITEM ,"SUM_FUNC_ITEM"},
            {Item::STRING_ITEM ,"STRING_ITEM"},
            {Item::INT_ITEM ,"INT_ITEM"},
            {Item::REAL_ITEM ,"REAL_ITEM"},
            {Item::NULL_ITEM ,"NULL_ITEM"},
            {Item::VARBIN_ITEM ,"VARBIN_ITEM"},
            {Item::COPY_STR_ITEM ,"COPY_STR_ITEM"},
            {Item::FIELD_AVG_ITEM ,"FIELD_AVG_ITEM"},
            {Item::DEFAULT_VALUE_ITEM ,"DEFAULT_VALUE_ITEM"},
            {Item::PROC_ITEM ,"PROC_ITEM"},
            {Item::COND_ITEM ,"COND_ITEM"},
            {Item::REF_ITEM ,"REF_ITEM"},
            {Item::FIELD_STD_ITEM ,"FIELD_STD_ITEM"},
            {Item::FIELD_VARIANCE_ITEM ,"FIELD_VARIANCE_ITEM"},
            {Item::INSERT_VALUE_ITEM ,"INSERT_VALUE_ITEM"},
            {Item::SUBSELECT_ITEM ,"SUBSELECT_ITEM"},
            {Item::ROW_ITEM ,"ROW_ITEM"},
            {Item::CACHE_ITEM ,"CACHE_ITEM"},
            {Item::TYPE_HOLDER ,"TYPE_HOLDER"},
            {Item::PARAM_ITEM ,"PARAM_ITEM"},
            {Item::TRIGGER_FIELD_ITEM ,"TRIGGER_FIELD_ITEM"},
            {Item::DECIMAL_ITEM ,"DECIMAL_ITEM"},
            {Item::XPATH_NODESET ,"XPATH_NODESET"},
            {Item::XPATH_NODESET_CMP ,"XPATH_NODESET_CMP"},
            {Item::VIEW_FIXER_ITEM ,"VIEW_FIXER_ITEM"}
        };
    }

    namespace datatypetrans{
        std::map<int,string> trans{
                {enum_field_types::MYSQL_TYPE_DECIMAL,"MYSQL_TYPE_DECIMAL"},
                {enum_field_types::MYSQL_TYPE_TINY,"MYSQL_TYPE_TINY"},
                {enum_field_types::MYSQL_TYPE_SHORT,"MYSQL_TYPE_SHORT"},
                {enum_field_types::MYSQL_TYPE_LONG,"MYSQL_TYPE_LONG"},
                {enum_field_types::MYSQL_TYPE_FLOAT,"MYSQL_TYPE_FLOAT"},
                {enum_field_types::MYSQL_TYPE_DOUBLE,"MYSQL_TYPE_DOUBLE"},
                {enum_field_types::MYSQL_TYPE_NULL,"MYSQL_TYPE_NULL"},
                {enum_field_types::MYSQL_TYPE_TIMESTAMP,"MYSQL_TYPE_TIMESTAMP"},
                {enum_field_types::MYSQL_TYPE_LONGLONG,"MYSQL_TYPE_LONGLONG"},
                {enum_field_types::MYSQL_TYPE_INT24,"MYSQL_TYPE_INT24"},
                {enum_field_types::MYSQL_TYPE_DATE,"MYSQL_TYPE_DATE"},
                {enum_field_types::MYSQL_TYPE_TIME,"MYSQL_TYPE_TIME"},
                {enum_field_types::MYSQL_TYPE_DATETIME,"MYSQL_TYPE_DATETIME"},
                {enum_field_types::MYSQL_TYPE_YEAR,"MYSQL_TYPE_YEAR"},
                {enum_field_types::MYSQL_TYPE_NEWDATE,"MYSQL_TYPE_NEWDATE"},
                {enum_field_types::MYSQL_TYPE_VARCHAR,"MYSQL_TYPE_VARCHAR"},
                {enum_field_types::MYSQL_TYPE_BIT,"MYSQL_TYPE_BIT"},
                {enum_field_types::MYSQL_TYPE_NEWDECIMAL,"MYSQL_TYPE_NEWDECIMAL"},
                {enum_field_types::MYSQL_TYPE_ENUM,"MYSQL_TYPE_ENUM"},
                {enum_field_types::MYSQL_TYPE_SET,"MYSQL_TYPE_SET"},
                {enum_field_types::MYSQL_TYPE_TINY_BLOB,"MYSQL_TYPE_TINY_BLOB"},
                {enum_field_types::MYSQL_TYPE_MEDIUM_BLOB,"MYSQL_TYPE_MEDIUM_BLOB"},
                {enum_field_types::MYSQL_TYPE_LONG_BLOB,"MYSQL_TYPE_LONG_BLOB"},
                {enum_field_types::MYSQL_TYPE_BLOB,"MYSQL_TYPE_BLOB"},
                {enum_field_types::MYSQL_TYPE_VAR_STRING,"MYSQL_TYPE_VAR_STRING"},
                {enum_field_types::MYSQL_TYPE_STRING,"MYSQL_TYPE_STRING"},
                {enum_field_types::MYSQL_TYPE_GEOMETRY,"MYSQL_TYPE_GEOMETRY"}
        };
    }

    namespace fieldflagtrans{
        std::map<int,string> trans{
                {1,"NOT_NULL_FLAG"},
                {2,"PRI_KEY_FLAG"},
                {4,"UNIQUE_KEY_FLAG"},
                {8,"MULTIPLE_KEY_FLAG"},
                {16,"BLOB_FLAG"},
                {32,"UNSIGNED_FLAG"},
                {64,"ZEROFILL_FLAG"},
                {128,"BINARY_FLAG"},
                {256,"ENUM_FLAG"},
                {512,"AUTO_INCREMENT_FLAG"},
                {1024,"TIMESTAMP_FLAG"},
                {2048,"SET_FLAG"},
                {4096,"NO_DEFAULT_VALUE_FLAG"},
                {8192,"ON_UPDATE_NOW_FLAG"},
                {32768,"NUM_FLAG"},
                {16384,"PART_KEY_FLAG"},
                {32768,"GROUP_FLAG"},
                {65536,"UNIQUE_FLAG"},
                {131072,"BINCMP_FLAG"},
                {(1<<18),"GET_FIXED_FIELDS_FLAG"},
                {(1<<19),"FIELD_IN_PART_FUNC_FLAG"},
                {(1<<20),"FIELD_IN_ADD_INDEX"},
                {(1<<21),"FIELD_IS_RENAMED"},
                {22,"FIELD_FLAGS_STORAGE_MEDIA"},
                {24,"FIELD_FLAGS_COLUMN_FORMAT"},
                {(1<<26),"ENC_FLAG"}
        };
    }
}








namespace show{


std::unique_ptr<query_parse> getLex(std::string origQuery) {
    std::unique_ptr<query_parse> p;
    try {
        std::string query(origQuery);
        std::string db("tdb");
        p = std::unique_ptr<query_parse>(new query_parse(db,query));
    }catch (char const* a){
        const std::string s(a);
        std::cout<<"error string: "<<s<<std::endl;
        return p;
    }
    catch (...){
        std::cout<<"error"<<std::endl;
        return p;
    }
    return p;
}




std::string
empty_if_null(const char *const p) {       
    if (p) return std::string(p);
    return std::string(""); 
}

/*
function 'draw is a wrapper for standard output'
*/
void showKeyList(LEX *lex) {
    draw(string("lex->alter_info.key_list: "),GREEN);
    auto it =
             List_iterator<Key>(lex->alter_info.key_list);
    while(auto cur = it++){
        //cur is the key, and the key either has a name or is an empty string
        draw(string("Key->name: "),(convert_lex_str(cur->name)));
        switch(cur->type){
            case Key::PRIMARY:
            case Key::UNIQUE:
            case Key::MULTIPLE:
            case Key::FULLTEXT:
            case Key::SPATIAL:{
                draw(string("Key->type: "),(keytrans::trans[cur->type]));
                auto it_columns = List_iterator<Key_part_spec>(cur->columns);
                while(auto go = it_columns++){
                    draw(string("Key->columns: "),(convert_lex_str(go->field_name)));
                }
                break;
            }
            case Key::FOREIGN_KEY:{
                draw(string("Key->type: FOREIGN"));
                //process ref_columns
                auto it_ref_columns = List_iterator<Key_part_spec>(((Foreign_key*)cur)->ref_columns);
                while(auto cur_ref_columns=it_ref_columns++){
                    draw(string("Foreign_key->ref_columns->field_name: "),(convert_lex_str(cur_ref_columns->field_name)));
                }
                //process ref table
                Table_ident* ref_table = ((Foreign_key*)cur)->ref_table;
                draw(string("Foreign_key->ref_table->db: "),(convert_lex_str(ref_table->db)),
                      string("Foreign_key->ref_table->table: "),(convert_lex_str(ref_table->table)));
                break;
            }
            default:{

            }
        }
    }
}

void showCreateFiled(LEX* lex){
    draw(string("lex->alter_info.create_list"),GREEN);
    auto it =
                List_iterator<Create_field>(lex->alter_info.create_list);
    while(auto cf = it++){
        string name = std::string(cf->field_name);
        enum_field_types sqltype = cf->sql_type;
        long len = cf->length;
        draw(name,datatypetrans::trans[sqltype]);
        draw(string("cf->flags: "),std::to_string(cf->flags));
        for(auto item:fieldflagtrans::trans){
            int flag = item.first;
            if((cf->flags&flag)==flag){
                draw(item.second);
            }

        }
        
    }
    
}    



void showTableList(LEX *lex){
    draw(string("lex->select_lex.table_list.first: "),GREEN);
    assert(lex->select_lex.table_list.first);
    string db,table;
    db = show::empty_if_null(lex->select_lex.table_list.first->db);
    table = show::empty_if_null(lex->select_lex.table_list.first->table_name);
    draw(string("lex->select_lex.table_list.first->db: "),db);
    draw(string("lex->select_lex.table_list.first->table_name: "),table);
    draw(string("lex->select_lex.table_list.elements: "),std::to_string(lex->select_lex.table_list.elements));


}



}





namespace rewrite{
string
lexToQuery(const LEX &lex) {
    std::ostringstream o;
    o << const_cast<LEX &>(lex);
    return o.str();
}




}




