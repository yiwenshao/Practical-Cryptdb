#include <memory>

#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>
#include <main/schema.hh>
#include <parser/lex_util.hh>
#include <parser/stringify.hh>
#include <util/enum_text.hh>

extern CItemTypesDir itemTypes;

void
optimize(Item ** const i, Analysis &a) {
   //TODO
}


// this function should be called at the root of a tree of items
// that should be rewritten
Item *
rewrite(const Item &i, const EncSet &req_enc, Analysis &a) {
    const std::unique_ptr<RewritePlan> &rp =
        constGetAssert(a.rewritePlans, &i);
    const EncSet solution = rp->es_out.intersect(req_enc);
    // FIXME: Use version that takes reason, expects 0 children,
    // and lets us indicate what our EncSet does have.
    TEST_NoAvailableEncSet(solution, i.type(), req_enc, rp->r.why,
                           std::vector<std::shared_ptr<RewritePlan> >());

    return itemTypes.do_rewrite(i, solution.chooseOne(), *rp.get(), a);
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t, const Analysis &a) {
    // Table name can only be empty when grouping a nested join.
    assert(t->table_name || t->nested_join);
    if (t->table_name) {
        const std::string plain_name =
            std::string(t->table_name, t->table_name_length);
        // Don't use Analysis::getAnonTableName(...) as it respects
        // aliases and if a table has been aliased with 'plain_name'
        // it will give us the wrong name.
        TEST_DatabaseDiscrepancy(t->db, a.getDatabaseName());
        const std::string anon_name =
            a.translateNonAliasPlainToAnonTableName(t->db, plain_name);
        return rewrite_table_list(t, anon_name);
    } else {
        return copyWithTHD(t);
    }
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t,
                   const std::string &anon_name)
{
    TABLE_LIST *const new_t = copyWithTHD(t);
    new_t->table_name = make_thd_string(anon_name);
    new_t->table_name_length = anon_name.size();
    if (false == new_t->is_alias) {
        new_t->alias = make_thd_string(anon_name);
    }
    new_t->next_local = NULL;

    return new_t;
}


// @if_exists: defaults to false; it is necessary to facilitate
//   'DROP TABLE IF EXISTS'
SQL_I_List<TABLE_LIST>
rewrite_table_list(const SQL_I_List<TABLE_LIST> &tlist, Analysis &a,
                   bool if_exists)
{
    if (!tlist.elements) {
        return SQL_I_List<TABLE_LIST>();
    }

    TABLE_LIST * tl;
    TEST_DatabaseDiscrepancy(tlist.first->db, a.getDatabaseName());
    if (if_exists && !a.nonAliasTableMetaExists(tlist.first->db,
                                                tlist.first->table_name)) {
       tl = copyWithTHD(tlist.first);
    } else {
       tl = rewrite_table_list(tlist.first, a);
    }

    const SQL_I_List<TABLE_LIST> * const new_tlist =
        oneElemListWithTHD<TABLE_LIST>(tl);

    TABLE_LIST * prev = tl;
    for (TABLE_LIST *tbl = tlist.first->next_local; tbl;
         tbl = tbl->next_local) {
        TABLE_LIST * new_tbl;
        TEST_DatabaseDiscrepancy(tbl->db, a.getDatabaseName());
        if (if_exists && !a.nonAliasTableMetaExists(tbl->db,
                                                    tbl->table_name)) {
            new_tbl = copyWithTHD(tbl);
        } else {
            new_tbl = rewrite_table_list(tbl, a);
        }

        prev->next_local = new_tbl;
        prev = new_tbl;
    }
    prev->next_local = NULL;

    return *new_tlist;
}

List<TABLE_LIST>
rewrite_table_list(List<TABLE_LIST> tll, Analysis &a) {
    List<TABLE_LIST> * const new_tll = new List<TABLE_LIST>();

    List_iterator<TABLE_LIST> join_it(tll);

    for (;;) {
        const TABLE_LIST * const t = join_it++;
        if (!t) {
            break;
        }

        TABLE_LIST * const new_t = rewrite_table_list(t, a);
        new_tll->push_back(new_t);

        if (t->nested_join) {
            new_t->nested_join->join_list =
                rewrite_table_list(t->nested_join->join_list, a);
            return *new_tll;
        }

        if (t->on_expr) {
            new_t->on_expr = rewrite(*t->on_expr, PLAIN_EncSet, a);
        }
    }

    return *new_tll;
}

RewritePlan *
gather(const Item &i, Analysis &a)
{
    return itemTypes.do_gather(i, a);
}

void
gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a)
{
    a.rewritePlans[&i] = std::unique_ptr<RewritePlan>(gather(i, a));
}

std::vector<std::tuple<std::vector<std::string>, Key::Keytype> >
collectKeyData(const LEX &lex)
{
    std::vector<std::tuple<std::vector<std::string>, Key::Keytype> > output;

    auto key_it =
        RiboldMYSQL::constList_iterator<Key>(lex.alter_info.key_list);
    for (;;) {
        const Key *const key = key_it++;
        if (nullptr == key) {
            break;
        }

        // collect columns
        std::vector<std::string> columns;
        auto col_it =
            RiboldMYSQL::constList_iterator<Key_part_spec>(key->columns);
        for (;;) {
            const Key_part_spec *const key_part = col_it++;
            if (nullptr == key_part) {
                break;
            }

            columns.push_back(convert_lex_str(key_part->field_name));
        }

        output.push_back(std::make_tuple(columns, key->type));
    }

    return output;
}

//TODO(raluca) : figure out how to create Create_field from scratch
// and avoid this chaining and passing f as an argument
static Create_field *
get_create_field(const Analysis &a, Create_field * const f,
                 const OnionMeta &om)
{
    const std::string name = om.getAnonOnionName();
    Create_field *new_cf = f;

    // Default value is handled during INSERTion.
    auto save_default = f->def;
    f->def = nullptr;

    const auto &enc_layers = a.getEncLayers(om);
    assert(enc_layers.size() > 0);
    for (const auto &it : enc_layers) {
        const Create_field * const old_cf = new_cf;
        new_cf = it->newCreateField(*old_cf, name);
    }

    // Restore the default so we don't memleak it.
    f->def = save_default;
    return new_cf;
}

// NOTE: The fields created here should have NULL default pointers
// as such is handled during INSERTion.
std::vector<Create_field *>
rewrite_create_field(const FieldMeta * const fm,
                     Create_field * const f, const Analysis &a)
{
    LOG(cdb_v) << "in rewrite create field for " << *f;

    assert(fm->getChildren().size() > 0);

    std::vector<Create_field *> output_cfields;

    // Don't add the default value to the schema.
    Item *const save_def = f->def;
    f->def = NULL;

    // create each onion column
    for (auto oit : fm->orderedOnionMetas()) {
        OnionMeta * const om = oit.second;
        Create_field * const new_cf = get_create_field(a, f, *om);

        output_cfields.push_back(new_cf);
    }

    // create salt column
    if (fm->getHasSalt()) {
        THD * const thd         = current_thd;
        Create_field * const f0 = f->clone(thd->mem_root);
        f0->field_name          = thd->strdup(fm->getSaltName().c_str());
        // Salt is unsigned and is not AUTO_INCREMENT.
        // > salt can only be NOT NULL if column is NOT NULL
        f0->flags               = (f0->flags | UNSIGNED_FLAG)
                                  & ~AUTO_INCREMENT_FLAG;
        f0->sql_type            = MYSQL_TYPE_LONGLONG;
        f0->length              = 8;

        output_cfields.push_back(f0);
    }

    // Restore the default to the original Create_field parameter.
    f->def = save_def;
    //for(auto item:output_cfields){
    //}

    return output_cfields;
}

std::vector<onion>
getOnionIndexTypes()
{
    return std::vector<onion>({oOPE, oDET, oPLAIN});
}

static std::string
getOriginalKeyName(const Key &key)
{
    if (Key::PRIMARY == key.type) {
        return "PRIMARY";
    }

    std::string out_name = convert_lex_str(key.name);

    //TEST_TextMessageError(out_name.size() > 0,
    //                      "Non-Primary keys can not have blank name!");
    if(out_name.size()==0){
        out_name="nonekey";
    }

    return out_name;
}

/*
static std::vector<Key *>
rewrite_key(const TableMeta &tm, const Key &key, const Analysis &a)
{
    std::vector<Key *> output_keys;

    //从左到右分别是三种类型: oOPE, oDET, oPLAIN, 对于每个语句的index都是这样
    //比如自己要alter table add 的index, 其对应index的名字, 以及相关的column信息
    //比如create的时候, 产生的index, 其对应的名字以及相关的columns, 这样包含在key里面了
    const std::vector<onion> key_onions = getOnionIndexTypes();
    for (auto onion_it : key_onions) {
        const onion o = onion_it;
        THD* cthd = current_thd;
        //原始key的拷贝
        Key *const new_key = key.clone(cthd->mem_root);
        //通过key的原始名字+onion+tm哈希获得新的key名字,用的是std::hash<string>算法.
        // Set anonymous name.
        const std::string new_name =
            a.getAnonIndexName(tm, getOriginalKeyName(key), o);
        //设置key的新名字, 以及新的key对应的column清空??
        new_key->name = string_to_lex_str(new_name);
        new_key->columns.empty();

        //遍历原始的key的columns, 改写, 变成新key的colukns
        // Set anonymous columns.
        auto col_it =
            RiboldMYSQL::constList_iterator<Key_part_spec>(key.columns);
        for (;;) {
            const Key_part_spec *const key_part = col_it++;
            if (NULL == key_part) {
                output_keys.push_back(new_key);
                break;
            }
            //复制一个column信息
            Key_part_spec *const new_key_part = copyWithTHD(key_part);
            //原始的key_part, 也就是一个column, 里面取一个field_name出来
            const std::string field_name =
                convert_lex_str(new_key_part->field_name);
            //通过column查tablemeta, 然后差当前的onion在不在那个tablemeta里面
            // > the onion may not exist; ie oPLAIN with SENSITIVE and not
            // an AUTO INCREMENT column
            const FieldMeta &fm = a.getFieldMeta(tm, field_name);
            const OnionMeta *const om = fm.getOnionMeta(o);
            if (NULL == om) {
                break;
            }
            //如果存在, 那么新的column的名字就是洋葱的名字, 然后new_key的column就确定了
            //可以看到, 对于索引里面的每个field, 都选择一个洋葱, 如果没有合适的, 则不选择.
            new_key_part->field_name =
                string_to_lex_str(om->getAnonOnionName());
            new_key->columns.push_back(new_key_part);
        }
    }

    //上面, 对于一个key来说, 可以查三个洋葱
    //对于每个洋葱,都可能构造一个key, 这是根据这个key对应的columns里面有没有洋葱, 有则选进来. 所以, 列扩展了以后, 索引也扩展了.
    //主键特殊处理, 之根据一个洋葱构造.
    // Only create one PRIMARY KEY.
    if (Key::PRIMARY == key.type) {
        if (output_keys.size() > 0) {
            return std::vector<Key *>({output_keys.front()});
        }
    }

    return output_keys;
}
*/


/*
only rewrite normal keys here, do not process foreign keys.

*/
static std::vector<Key *>
rewrite_key1(const TableMeta &tm, const Key &key, const Analysis &a){

    //leave foreign key unchanged
    std::vector<Key *> output_keys;
    if(key.type==Key::FOREIGN_KEY){
        THD* cthd = current_thd;
        Key *const new_key = key.clone(cthd->mem_root);
        output_keys.push_back(new_key);
        return output_keys;
    }

    //从左到右分别是三种类型: oOPE, oDET, oPLAIN, 对于每个语句的index都是这样
    //比如自己要alter table add 的index, 其对应index的名字, 以及相关的column信息
    //比如create的时候, 产生的index, 其对应的名字以及相关的columns, 这样包含在key里面了
    const std::vector<onion> key_onions = getOnionIndexTypes();
    for (auto onion_it : key_onions) {
        const onion o = onion_it;
        THD* cthd = current_thd;
        //原始key的拷贝
        Key *const new_key = key.clone(cthd->mem_root);
        //通过key的原始名字+onion+tm哈希获得新的key名字,用的是std::hash<string>算法.
        // Set anonymous name.
        const std::string new_name =
            a.getAnonIndexName(tm, getOriginalKeyName(key), o);
        //设置key的新名字, 以及新的key对应的column清空??
        new_key->name = string_to_lex_str(new_name);
        new_key->columns.empty();

        //遍历原始的key的columns, 改写, 变成新key的colukns
        // Set anonymous columns.
        auto col_it =
            RiboldMYSQL::constList_iterator<Key_part_spec>(key.columns);
        for (;;) {
            const Key_part_spec *const key_part = col_it++;
            if (NULL == key_part) {
                output_keys.push_back(new_key);
                break;
            }
            //复制一个column信息
            Key_part_spec *const new_key_part = copyWithTHD(key_part);
            //原始的key_part, 也就是一个column, 里面取一个field_name出来
            const std::string field_name =
                convert_lex_str(new_key_part->field_name);
            //通过column查tablemeta, 然后差当前的onion在不在那个tablemeta里面
            // > the onion may not exist; ie oPLAIN with SENSITIVE and not
            // an AUTO INCREMENT column
            const FieldMeta &fm = a.getFieldMeta(tm, field_name);
            const OnionMeta *const om = fm.getOnionMeta(o);
            if (NULL == om) {
                break;
            }
            //如果存在, 那么新的column的名字就是洋葱的名字, 然后new_key的column就确定了
            //可以看到, 对于索引里面的每个field, 都选择一个洋葱, 如果没有合适的, 则不选择.
            new_key_part->field_name =
                string_to_lex_str(om->getAnonOnionName());
            new_key->columns.push_back(new_key_part);
        }
    }

    //上面, 对于一个key来说, 可以查三个洋葱
    //对于每个洋葱,都可能构造一个key, 这是根据这个key对应的columns里面有没有洋葱, 有则选进来. 所以, 列扩展了以后, 索引也扩展了.
    //主键特殊处理, 之根据一个洋葱构造.
    // Only create one PRIMARY KEY.
    if (Key::PRIMARY == key.type) {
        if (output_keys.size() > 0) {
            return std::vector<Key *>({output_keys.front()});
        }
    }
    return output_keys;
}




// 'seed_lex' and 'out_lex' can be the same object.
void
highLevelRewriteKey(const TableMeta &tm, const LEX &seed_lex,
                    LEX *const out_lex, const Analysis &a){
    assert(out_lex);

    // Add each new index.
    auto key_it =
        List_iterator<Key>(const_cast<LEX &>(seed_lex).alter_info.key_list);
    out_lex->alter_info.key_list =
        accumList<Key>(key_it,
            [&tm, &a] (List<Key> out_list, const Key *const key) {
                // -----------------------------
                //         Rewrite INDEX
                // -----------------------------
                auto new_keys = rewrite_key1(tm, *key, a);
                out_list.concat(vectorToListWithTHD(new_keys));

                return out_list;
        });

    return;
}

void 
highLevelRewriteForeignKey(const TableMeta &tm, const LEX &seed_lex,
            LEX *const out_lex, const Analysis &a,std::string tbname){
    std::string dbname = a.getDatabaseName();
    auto it =
             List_iterator<Key>(out_lex->alter_info.key_list);
    std::vector<Key *> output_keys;

    while(auto cur = it++){
        if(cur->type== Key::FOREIGN_KEY){
           THD* cthd = current_thd;
            //process current names
            Key* const new_key = cur->clone(cthd->mem_root);
            std::string new_name = "newfkname";
            new_key->name = string_to_lex_str(new_name);

            //process current columns
            auto col_it_cur = List_iterator<Key_part_spec>((cur->columns));

            new_key->columns.empty();

            while(1){
                const Key_part_spec *const key_part = col_it_cur++;
                if(NULL == key_part){
                    break;
                }
                Key_part_spec *const new_key_part = copyWithTHD(key_part);
                std::string field_name =
                convert_lex_str(new_key_part->field_name);
                //get current field name, and then replace it with one onionname here                
                //currently we choose det onion, without caring about the layers
                //OnionMeta *om = a.getOnionMeta2(dbname,tbname,field_name,oDET);
                OnionMeta *om = tm.getChild(IdentityMetaKey(field_name))->getOnionMeta(oOPE);
                assert(om!=NULL);                
                field_name=om->getAnonOnionName();
                new_key_part->field_name = string_to_lex_str(field_name);
                new_key->columns.push_back(new_key_part);
            }

            //process ref tables
            Table_ident* ref_table = ((Foreign_key*)cur)->ref_table;

            std::string ref_table_name = std::string(ref_table->table.str,ref_table->table.length);
            TableMeta &rtm = a.getTableMeta(a.getDatabaseName(),ref_table_name);

            std::string ref_table_annoname = rtm.getAnonTableName();
            ref_table->table = string_to_lex_str(ref_table_annoname);

            //process ref columns
            auto col_it =
            List_iterator<Key_part_spec>(((Foreign_key*)cur)->ref_columns);
            ((Foreign_key*)new_key)->ref_columns.empty();
            while(1){
                const Key_part_spec *const key_part = col_it++;
                if(NULL == key_part){
                    break;
                }
                Key_part_spec *const new_key_part = copyWithTHD(key_part);
                std::string field_name =
                convert_lex_str(new_key_part->field_name);
                //update field name here
                OnionMeta * om = a.getOnionMeta2(dbname,ref_table_name,field_name,oOPE);
                assert(om!=NULL);
                field_name=om->getAnonOnionName();
                new_key_part->field_name = string_to_lex_str(field_name);
                ((Foreign_key*)new_key)->ref_columns.push_back(new_key_part);
            }
            output_keys.push_back(new_key);
        }else{
            THD* cthd = current_thd;
            Key* const new_key = cur->clone(cthd->mem_root);
            output_keys.push_back(new_key);
        }
    }
       
    out_lex->alter_info.key_list = *vectorToListWithTHD(output_keys);
   

}




std::string
bool_to_string(bool b)
{
    if (true == b) {
        return "TRUE";
    } else {
        return "FALSE";
    }
}

bool
string_to_bool(const std::string &s)
{
    if (s == std::string("TRUE") || s == std::string("1")) {
        return true;
    } else if (s == std::string("FALSE") || s == std::string("0")) {
        return false;
    } else {
        throw "unrecognized string in string_to_bool!";
    }
}

static bool
isUnique(const std::string &name,
         const std::vector<std::tuple<std::vector<std::string>, Key::Keytype> > &
             key_data)
{
    bool unique = false;
    for (const auto &it : key_data) {
        const auto &found =
            std::find(std::get<0>(it).begin(), std::get<0>(it).end(), name);
        if (found != std::get<0>(it).end()
            && (std::get<1>(it) == Key::PRIMARY || std::get<1>(it) == Key::UNIQUE ||std::get<1>(it)==Key::FOREIGN_KEY)) {
            unique = true;
            break;
        }
    }

    return unique;
}


//from one filed to multiple fields. the fields are automatically transformed to unsigned. This is called in createHandler.
List<Create_field>
createAndRewriteField(Analysis &a, Create_field * const cf,
                      TableMeta *const tm, bool new_table,
                      const std::vector<std::tuple<std::vector<std::string>,
                                        Key::Keytype> >
                          &key_data,
                      List<Create_field> &rewritten_cfield_list)
{
    // we only support the creation of UNSIGNED fields
    //add this to support plaintext data and decimal
    if(cf->sql_type != enum_field_types::MYSQL_TYPE_DECIMAL && 
       cf->sql_type != enum_field_types::MYSQL_TYPE_FLOAT &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_DOUBLE &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_TIMESTAMP &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_DATE &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_TIME &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_DATETIME &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_YEAR &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_NEWDATE &&
       cf->sql_type != enum_field_types::MYSQL_TYPE_NEWDECIMAL
     )
     cf->flags = cf->flags | UNSIGNED_FLAG;

    const std::string &name = std::string(cf->field_name);
    std::unique_ptr<FieldMeta>
        fm(new FieldMeta(*cf, a.getMasterKey().get(),
                         a.getDefaultSecurityRating(), tm->leaseCount(),
                         isUnique(name, key_data)));
    // -----------------------------
    //         Rewrite FIELD
    // -----------------------------
    //for each onion, we have new fields and salts, salts have long long type
    const auto new_fields = rewrite_create_field(fm.get(), cf, a);
    rewritten_cfield_list.concat(vectorToListWithTHD(new_fields));
    // -----------------------------
    //         Update FIELD
    // -----------------------------

    // Here we store the key name for the first time. It will be applied
    // after the Delta is read out of the database.
    if (true == new_table) {
        tm->addChild(IdentityMetaKey(name), std::move(fm));
    } else {
        a.deltas.push_back(std::unique_ptr<Delta>(
                                new CreateDelta(std::move(fm), *tm,
                                                IdentityMetaKey(name))));
        a.deltas.push_back(std::unique_ptr<Delta>(
               new ReplaceDelta(*tm,
                                a.getDatabaseMeta(a.getDatabaseName()))));
    }
    return rewritten_cfield_list;
}

//TODO: which encrypt/decrypt should handle null?
Item *
encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    const Analysis &a, uint64_t IV) {
    assert(!RiboldMYSQL::is_null(i));
    //这里是onionMeta中的vector, enclayers.也就是洋葱不同层次的加解密通过Onionmeta以及
    //encLary中的加解密算法来完成.
    const auto &enc_layers = a.getEncLayers(om);
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    const Item *enc = &i;
    Item *new_enc = NULL;
    //这段代码体现了层次加密,也就是说, 通过IV,每个洋葱的层次通过enclayer来表示
    //直接调用其加密和解密函数, 就可以完成加密工作. 加密以后获得的是Item,最后返回加密以后的结果
    for (const auto &it : enc_layers) {
        LOG(encl) << "encrypt layer "
                  << TypeText<SECLEVEL>::toText(it->level()) << "\n";
        new_enc = it->encrypt(*enc, IV);
        assert(new_enc);
        enc = new_enc;
    }
    // @i is const, do we don't want the caller to modify it accidentally.
    assert(new_enc && new_enc != &i);
    return new_enc;
}

std::string
escapeString(const std::unique_ptr<Connect> &c,
             const std::string &escape_me)
{
    const unsigned int escaped_length = escape_me.size() * 2 + 1;
    std::unique_ptr<char, void (*)(void *)>
        escaped(new char[escaped_length], &operator delete []);
    c->real_escape_string(escaped.get(), escape_me.c_str(),
                          escape_me.size());

    return std::string(escaped.get());
}

void
encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
                        uint64_t IV, Analysis &a, std::vector<Item*> *l)
{
    for (auto it : fm.orderedOnionMetas()) {
       
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        //一个fieldmeta表示一个field, 内部的不同洋葱表现在onionMeta,每个onionMeta的不同层次表现
        //在enclyer. 而保持的时候, 是onometekey,onoinmeta这种pair来让我们知道这个onionMeta是哪种
        //枚举的洋葱类型.
        l->push_back(encrypt_item_layers(i, o, *om, a, IV));
    }
}

void
typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                            Analysis &a, std::vector<Item *> *l) {


    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;

    encrypt_item_all_onions(i, fm, salt, a, l);

    //对于每种类型, 除了保存加密的洋葱, 还把fm中的salt也变成Int类型保存起来了, 所以会出现奇怪的多了一组数据的情况, 就看
    //这个东西是什么时候应用.
    if (fm.getHasSalt()) {
        l->push_back(new Item_int(static_cast<ulonglong>(salt)));
    }
}

/*
 * connection ids can be longer than 32 bits
 * http://dev.mysql.com/doc/refman/5.1/en/mysql-thread-id.html
 */
static bool
getConnectionID(const std::unique_ptr<Connect> &c,
                unsigned long long *const connection_id)
{
    std::unique_ptr<DBResult> dbres;
    const std::string &query = " SELECT CONNECTION_ID();";
    RETURN_FALSE_IF_FALSE(c->execute(query, &dbres));
    RETURN_FALSE_IF_FALSE(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    *connection_id =
        strtoull(std::string(row[0], l[0]).c_str(), NULL, 10);
    return true;
}


std::string
getDefaultDatabaseForConnection(const std::unique_ptr<Connect> &c)
{
    unsigned long long thread_id;
    TEST_TextMessageError(getConnectionID(c, &thread_id),
                          "failed to get connection id!");
    std::string out_name;
    TEST_TextMessageError(retrieveDefaultDatabase(thread_id, c, &out_name),
                          "failed to retrieve default database!");

    return out_name;
}

bool
retrieveDefaultDatabase(unsigned long long thread_id,
                        const std::unique_ptr<Connect> &c,
                        std::string *const out_name)
{
    out_name->clear();

    std::unique_ptr<DBResult> dbres;
    //这里是多行的字符串.
    const std::string &query =
        " SELECT db FROM INFORMATION_SCHEMA.PROCESSLIST"
        "  WHERE id = " + std::to_string(thread_id) + ";";
    RETURN_FALSE_IF_FALSE(c->execute(query, &dbres));
    RETURN_FALSE_IF_FALSE(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    *out_name = std::string(row[0], l[0]);
    return true;
}

std::string
terminalEscape(const std::string &s)
{
    std::string out;
    for (auto it : s) {
        if (isprint(it)) {
            out.push_back(it);
            continue;
        }

        out.push_back('?');
    }

    return out;
}

void
prettyPrintQuery(const std::string &query)
{
    std::cout << std::endl << RED_BEGIN
              << "QUERY: " << COLOR_END << terminalEscape(query) << std::endl;
}

SECURITY_RATING
determineSecurityRating()
{
    const char *const secure = getenv("SECURE_CRYPTDB");
    if (secure && equalsIgnoreCase("FALSE", secure)) {
        return SECURITY_RATING::BEST_EFFORT;
    }
    
    return SECURITY_RATING::SENSITIVE;
}

bool
handleActiveTransactionPResults(const ResType &res)
{
    assert(res.success());
    assert(res.rows.size() == 1);

    const std::string &trx = ItemToString(*res.rows.front().front());
    assert("1" == trx || "0" == trx);
    return ("1" == trx);
}

