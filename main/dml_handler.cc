#include <functional>

#include <main/dml_handler.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/dispatcher.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>
#include <parser/lex_util.hh>
#include <util/onions.hh>
#include <util/yield.hpp>

extern CItemTypesDir itemTypes;

// Prototypes.
static void
process_field_value_pairs(List_iterator<Item> fd_it,
                          List_iterator<Item> val_it, Analysis &a);

static void
process_filters_lex(const st_select_lex &select_lex, Analysis &a);

static inline void
gatherAndAddAnalysisRewritePlanForFieldValuePair(const Item_field &field,
                                                 const Item &val,
                                                 Analysis &a);

static st_select_lex *
rewrite_filters_lex(const st_select_lex &select_lex, Analysis &a);



enum class
SIMPLE_UPDATE_TYPE {UNSUPPORTED, ON_DUPLICATE_VALUE,
                    SAME_VALUE, NEW_VALUE};

SIMPLE_UPDATE_TYPE
determineUpdateType(const Item &value_item, const FieldMeta &fm,
                    const EncSet &es);

void
handleUpdateType(SIMPLE_UPDATE_TYPE update_type, const EncSet &es,
                 const Item_field &field_item, const Item &value_item,
                 List<Item> *const res_fields,
                 List<Item> *const res_values, Analysis &a);

template <typename ContainerType>
void rewriteInsertHelper(const Item &i, const FieldMeta &fm, Analysis &a,
                         ContainerType *const append_list)
{
    std::vector<Item *> l;
    /*first look up the right class and then use the rewritting function of the specific data type
    for table student, the function typical_rewrite_insert_type in ANON is used finally. */
    itemTypes.do_rewrite_insert(i, fm, a, &l);
    for (auto it : l) {
        append_list->push_back(it);
    }
}


void InsertHandler::gather(Analysis &a, LEX *const lex) const {
        //only select xxx etc?不是的!!!
        process_select_lex(lex->select_lex, a);

        // -----------------------
        // ON DUPLICATE KEY UPDATE
        // -----------------------
        auto fd_it = List_iterator<Item>(lex->update_list);
        auto val_it = List_iterator<Item>(lex->value_list);
        //find plain for field and value rewrite?
        process_field_value_pairs(fd_it, val_it, a);

        return;
    }

    AbstractQueryExecutor * InsertHandler::rewrite(Analysis &a, LEX *const lex)
        const{
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

        //For insert, we can choose to specify field list or omit it.
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

        // -----------------
        //      Values
        // -----------------
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
                    //li pointer to items of lex->many_values 
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

        //for queries with ON DUPLICATE KEY UPDATE
        // -----------------------
        // ON DUPLICATE KEY UPDATE
        // -----------------------
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
        return new DMLQueryExecutor(*new_lex, a.rmeta);
    }




class UpdateHandler : public DMLHandler {
    virtual void gather(Analysis &a, LEX *lex) const {
        process_table_list(lex->select_lex.top_join_list, a);

        if (lex->select_lex.item_list.head()) {
            assert(lex->value_list.head());

            auto fd_it = List_iterator<Item>(lex->select_lex.item_list);
            auto val_it = List_iterator<Item>(lex->value_list);
            process_field_value_pairs(fd_it, val_it, a);
        }

        process_filters_lex(lex->select_lex, a);
    }

    virtual AbstractQueryExecutor *rewrite(Analysis &a, LEX *lex) const
    {
        LEX *const new_lex = copyWithTHD(lex);

        LOG(cdb_v) << "rewriting update \n";

        assert_s(lex->select_lex.item_list.head(),
                 "update needs to have item_list");

        // Rewrite table name
        new_lex->select_lex.top_join_list =
            rewrite_table_list(lex->select_lex.top_join_list, a);

        // Rewrite filters
        set_select_lex(new_lex,
                       rewrite_filters_lex(new_lex->select_lex, a));

        // Rewrite SET values
        assert(lex->select_lex.item_list.head());
        assert(lex->value_list.head());

        auto fd_it = List_iterator<Item>(lex->select_lex.item_list);
        auto val_it = List_iterator<Item>(lex->value_list);
        List<Item> res_fields, res_values;
        // Special Update?
        if (false == rewrite_field_value_pairs(fd_it, val_it, a, 
                                               &res_fields, &res_values)) {
            const auto plain_table =
                lex->select_lex.top_join_list.head()->table_name;
            const auto crypted_table =
                new_lex->select_lex.top_join_list.head()->table_name;
            AssignOnce<std::string> where_clause;
            if (lex->select_lex.where) {
                std::ostringstream where_stream;
                where_stream << " " << *lex->select_lex.where << " ";
                where_clause = where_stream.str();
            } else {
                where_clause = " TRUE ";
            }

            return new SpecialUpdateExecutor(plain_table, crypted_table,
                                             where_clause.get());
        }

        new_lex->select_lex.item_list = res_fields;
        new_lex->value_list = res_values;
        return new DMLQueryExecutor(*new_lex, a.rmeta);
    }
};

class DeleteHandler : public DMLHandler {
    virtual void gather(Analysis &a, LEX *const lex)
        const {
        process_select_lex(lex->select_lex, a);
    }

    virtual AbstractQueryExecutor *
        rewrite(Analysis &a, LEX *lex)
        const
    {
        LEX *const new_lex = copyWithTHD(lex);
        //这个因该是delete from的来源!
        new_lex->query_tables = rewrite_table_list(lex->query_tables, a);
        set_select_lex(new_lex,
                       rewrite_select_lex(new_lex->select_lex, a));

        return new DMLQueryExecutor(*new_lex, a.rmeta);
    }
};

class MultiDeleteHandler : public DMLHandler {
    virtual void gather(Analysis &a, LEX *const lex)
        const {
        process_select_lex(lex->select_lex, a);
    }

    virtual AbstractQueryExecutor *
        rewrite(Analysis &a, LEX *lex)
        const {
        LEX *const new_lex = copyWithTHD(lex);
        // the multidelete looks like this
        //  $ DELETE <LEX::auxiliary_...> FROM <LEX::query_tables>;
        // if query_tablez doesn't have an alias for a value it's going
        // to rewrite the table name (and also put this rewritten value
        // in the alias); the auxlist must then also rewrite it's value
        //
        // if query_tablez does have an alias it will leave the alias
        // and rewrite the real table name; the auxlist then needs to
        // leave it's value as _is_ because it's already the alias (in a
        // well formed query)
        //
        // the auxlist doesn't ``correctly'' set the TABLE_LIST::is_alias
        // parameter; we ``fix'' that here
        //
        // the corner case is when a table is aliased as it's real name
        //  $ DELETE a FROM a AS A
        // we resolve this by looking up the alias in Analysis
        //
        // our goal is to use aliases in the initial DELETE form, ON
        // clauses and WHERE clauses; while not using it in the JOIN
        // clauses; the JOIN clause should print the full field
        // (db.field.table) as well as the alias
        //
        // the problem is further complicated by a peculiarity of the
        // initial DELETE form; in most other clauses we can safely
        // do db.alias.field but in the DELETE form we _must_ do
        // alias.field.  therefore we have to tell the rewrite function
        // for Item_field that it should ``inject'' the alias name
        for (TABLE_LIST *tbl = lex->auxiliary_table_list.first;
             tbl;
             tbl = tbl->next_local) {

            assert(false == tbl->is_alias);
            // it's useful to do this style of alias check as well
            // because it doesn't require the database name
            if (strcmp(tbl->alias, tbl->table_name)) {
                tbl->is_alias = true;
            } else {
                const std::string &db =
                    std::string(tbl->db, tbl->db_length);
                TEST_DatabaseDiscrepancy(db, a.getDatabaseName());

                tbl->is_alias = a.isAlias(db, tbl->alias);
            }
        }
        // rewrite the DELETE form
        new_lex->auxiliary_table_list =
            rewrite_table_list(lex->auxiliary_table_list, a);

        // rewrite the ON/JOIN forms
        TABLE_LIST *new_query_tables = NULL;
        for (TABLE_LIST *tbl = lex->query_tables;
             tbl;
             tbl = tbl->next_local) {
            // JOIN form
            TABLE_LIST *const new_t = rewrite_table_list(tbl, a);

            // FIXME: look at rewrite_table_list and determine if we
            // can support
            TEST_TextMessageError(NULL == tbl->nested_join,
                                  "No nested joins in DELETE FROM");

            // ON form
            if (tbl->on_expr) {
                ScopedAssignment<bool>(&a.inject_alias, true,
                    [&new_t, &tbl, &a] ()
                    {
                        new_t->on_expr =
                            ::rewrite(*tbl->on_expr, PLAIN_EncSet, a);
                    });
            }

            // first iteration?
            if (NULL == new_query_tables) {
                new_query_tables = new_t;
            } else {
                new_query_tables->next_local = new_t;
            }
        }
        new_lex->query_tables = new_query_tables;

        set_select_lex(new_lex,
                       rewrite_select_lex(new_lex->select_lex, a));

        return new DMLQueryExecutor(*new_lex, a.rmeta);
    }
};

void 
SelectHandler::gather(Analysis &a, LEX *const lex)
    const{
    /*process the select field, that is the xxx in select xxx. also setup rewriteplain for fileds
      like having. the rewriteplain are encset, which is used in decryption*/
    process_select_lex(lex->select_lex, a);
}

AbstractQueryExecutor *
SelectHandler::rewrite(Analysis &a, LEX *lex)
    const{
    LEX *const new_lex = copyWithTHD(lex);

    //table list rewrite
    new_lex->select_lex.top_join_list =
        rewrite_table_list(lex->select_lex.top_join_list, a);

    SELECT_LEX *const select_lex_res = rewrite_select_lex(new_lex->select_lex, a);
    set_select_lex(new_lex,select_lex_res);

    return new DMLQueryExecutor(*new_lex, a.rmeta);
}

AbstractQueryExecutor *DMLHandler::
transformLex(Analysis &analysis, LEX *lex) const {
    this->gather(analysis, lex);
    return this->rewrite(analysis, lex);
}

// Helpers.

static void
process_order(const SQL_I_List<ORDER> &lst, Analysis &a)
{
    for (const ORDER *o = lst.first; o; o = o->next) {
        gatherAndAddAnalysisRewritePlan(**o->item, a);
    }
}

static void
process_filters_lex(const st_select_lex &select_lex, Analysis &a)
{
    if (select_lex.where) {
        gatherAndAddAnalysisRewritePlan(*select_lex.where, a);
    }

    if (select_lex.having) {
        gatherAndAddAnalysisRewritePlan(*select_lex.having, a);
    }

    process_order(select_lex.group_list, a);
    process_order(select_lex.order_list, a);
}

void
process_select_lex(const st_select_lex &select_lex, Analysis &a)
{
    /*select_lex.top_join_list is join list of the top level. The type is List<TABLE_LIST>. internally, 
      it uses two functions process_table_aliases(tll, a) and process_table_joins_and_derived(tll, a)
      to process the list. nested join is processed via recursion. and *on* queries are processed also. 
      if no join is used in the query, then this function is not used.
    */
    process_table_list(select_lex.top_join_list, a);

    /* process the itemlist, that is: List of columns and expressions: SELECT: Columns and
       expressions in the SELECT list. The type is List<Item>.
    */
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);
    int numOfItem = 0;
    for (;;) {
    /*not used in normal insert queries;
      processes id and name in the table student
    */
        const Item *const item = item_it++;
        if (!item)
            break;
        numOfItem++;
        gatherAndAddAnalysisRewritePlan(*item, a);
    }

    /*process select_lex.where and select_lex.having, all of which are of the type Item.
      rewriteplain is added for those items. Also, process_order is used internally to process
      select_lex.group_list and select_lex.order_list
    */
    process_filters_lex(select_lex, a);
}

static void
process_field_value_pairs(List_iterator<Item> fd_it,
                          List_iterator<Item> val_it, Analysis &a)
{
    for (;;) {
        const Item *const field_item = fd_it++;
        const Item *const value_item = val_it++;
        assert(!!field_item == !!value_item);
        if (!field_item) {
            break;
        }
        assert(field_item->type() == Item::FIELD_ITEM);
        const Item_field *const ifd =
            static_cast<const Item_field *>(field_item);
        gatherAndAddAnalysisRewritePlanForFieldValuePair(*ifd,
                                                         *value_item, a);
    }
}

// TODO:
// should be able to support general updates such as
// UPDATE table SET x = 2, y = y + 1, z = y+2, z =z +1, w = y, l = x
// this has a few corner cases, since y can only use HOM
// onion so does w, but not l

//analyzes an expression of the form field = val expression from
// an UPDATE
static inline void
gatherAndAddAnalysisRewritePlanForFieldValuePair(const Item_field &field,
                                                 const Item &val,
                                                 Analysis &a)
{
    a.rewritePlans[&val] = std::unique_ptr<RewritePlan>(gather(val, a));
    a.rewritePlans[&field] =
        std::unique_ptr<RewritePlan>(gather(field, a));

    //TODO: an optimization could be performed here to support more updates
    // For example: SET x = x+1, x = 2 --> no need to invalidate DET and OPE
    // onions because first SET does not matter
}

// FIXME: const Analysis
static SQL_I_List<ORDER> *
rewrite_order(Analysis &a, const SQL_I_List<ORDER> &lst,
              const EncSet &constr, const std::string &name)
{
    SQL_I_List<ORDER> *const new_lst = copyWithTHD(&lst);
    ORDER * prev = NULL;
    for (ORDER *o = lst.first; o; o = o->next) {
        const Item &i = **o->item;
        Item *const new_item = rewrite(i, constr, a);
        ORDER *const neworder = make_order(o, new_item);
        if (NULL == prev) {
            *new_lst = *oneElemListWithTHD(neworder);
        } else {
            prev->next = neworder;
        }
        prev = neworder;
    }

    return new_lst;
}
//order by, group by, where, having.
static st_select_lex *
rewrite_filters_lex(const st_select_lex &select_lex, Analysis & a)
{
    st_select_lex *const new_select_lex = copyWithTHD(&select_lex);

    // FIXME: Use const reference for list.
    new_select_lex->group_list =
        *rewrite_order(a, select_lex.group_list, EQ_EncSet, "group by");
    new_select_lex->order_list =
        *rewrite_order(a, select_lex.order_list, ORD_EncSet, "order by");

    if (select_lex.where) {
        set_where(new_select_lex, rewrite(*select_lex.where,
                                          PLAIN_EncSet, a));
    }
    // HACK: We only care about Analysis::item_cache from HAVING.
    a.item_cache.clear();
    if (select_lex.having) {
        set_having(new_select_lex, rewrite(*select_lex.having,
                                           PLAIN_EncSet, a));
    }

    return new_select_lex;
}

bool
rewrite_field_value_pairs(List_iterator<Item> fd_it,
                          List_iterator<Item> val_it, Analysis &a,
                          List<Item> *const res_fields,
                          List<Item> *const res_values) {
    for (;;) {
        const Item *const field_item = fd_it++;
        const Item *const value_item = val_it++;
        assert(!!field_item == !!value_item);
        if (!field_item) {
            break;
        }

        assert(field_item->type() == Item::FIELD_ITEM);
        const Item_field *const ifd =
            static_cast<const Item_field *>(field_item);
        FieldMeta &fm =
            a.getFieldMeta(a.getDatabaseName(), ifd->table_name,
                           ifd->field_name);

        const std::unique_ptr<RewritePlan> &rp_field =
            constGetAssert(a.rewritePlans, field_item);
        const std::unique_ptr<RewritePlan> &rp_value =
            constGetAssert(a.rewritePlans, value_item);

        const EncSet needed = EncSet(a, &fm);
        const EncSet r_es =
            rp_value->es_out.intersect(needed)
                            .intersect(rp_field->es_out);

        const SIMPLE_UPDATE_TYPE update_type =
            determineUpdateType(*value_item, fm, r_es);
        if (SIMPLE_UPDATE_TYPE::UNSUPPORTED == update_type) {
            return false;
        }

        handleUpdateType(update_type, r_es, *ifd, *value_item,
                         res_fields, res_values, a);
    }

    return true;
}

static void
addToReturn(ReturnMeta *const rm, int pos, const OLK &constr,
            bool has_salt, const std::string &name){
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    const int salt_pos = has_salt ? pos + 1 : -1;
    std::pair<int, ReturnField>
        pair(pos, ReturnField(false, name, constr, salt_pos));
    rm->rfmeta.insert(pair);
}

static void
addSaltToReturn(ReturnMeta *const rm, int pos)
{
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    std::pair<int, ReturnField>
        pair(pos, ReturnField(true, "", OLK::invalidOLK(), -1));
    rm->rfmeta.insert(pair);
}

//Item needs to be encrypted using the RewritePlain from the gather phase. The results will be put in newList, and Analysis is helper class.
static void
rewrite_proj(const Item &i, const RewritePlan &rp, Analysis &a,
             List<Item> *const newList) {
    AssignOnce<OLK> olk;
    AssignOnce<Item *> ir;

    if (i.type() == Item::Type::FIELD_ITEM) {
        const Item_field &field_i = static_cast<const Item_field &>(i);
        const auto &cached_rewritten_i = a.item_cache.find(&field_i);
        if (cached_rewritten_i != a.item_cache.end()) {
            ir = cached_rewritten_i->second.first;
            olk = cached_rewritten_i->second.second;
        } else {
            //rewrite=>do_rewrite_type for Item of type FIELD_ITEM
            ir = rewrite(i, rp.es_out, a);
            olk = rp.es_out.chooseOne();
        }
    } else {
        ir = rewrite(i, rp.es_out, a);
        olk = rp.es_out.chooseOne();
    }

    //Different form INSERT, SELECT needs only one onion.
    assert(ir.assigned() && ir.get());
    newList->push_back(ir.get());
    const bool use_salt = needsSalt(olk.get());

    // This line implicity handles field aliasing for at least some cases.
    // As i->name can/will be the alias.
    addToReturn(&a.rmeta, a.pos++, olk.get(), use_salt, i.name);

    if (use_salt) {
        TEST_TextMessageError(Item::Type::FIELD_ITEM == ir.get()->type(),
            "a projection requires a salt and is not a field; cryptdb"
            " does not currently support such behavior");
        const std::string &anon_table_name =
            static_cast<Item_field *>(ir.get())->table_name;
        const std::string &anon_field_name = olk.get().key->getSaltName();
        Item_field *const ir_field =
            make_item_field(*static_cast<Item_field *>(ir.get()),
                            anon_table_name, anon_field_name);
        newList->push_back(ir_field);
        addSaltToReturn(&a.rmeta, a.pos++);
    }
}

st_select_lex *
rewrite_select_lex(const st_select_lex &select_lex, Analysis &a)
{
    // rewrite_filters_lex must be called before rewrite_proj because
    // it is responsible for filling Analysis::item_cache which
    // rewrite_proj uses.
    st_select_lex *const new_select_lex = rewrite_filters_lex(select_lex, a);

    LOG(cdb_v) << "rewrite select lex input is "
               << select_lex << std::endl;
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);

    List<Item> newList;
    int numOfItem=0;
    //item的改写, 是写到newlist里面, 所以item本身不会有变化.
    for (;;) {

        const Item *const item = item_it++;
        if (!item)
            break;
        numOfItem++;
        rewrite_proj(*item,
                     *constGetAssert(a.rewritePlans, item).get(),
                     a, &newList);
    }

//    auto item_it_new =
//        RiboldMYSQL::constList_iterator<Item>(newList);
//    std::cout<<"rewrite#############" <<std::endl;
//    for(;;){
//        const Item *const item = item_it_new++;
//        if(!item) break;
//        std::cout<<"itemname: "<<item->name<<std::endl;
//     }

//    std::cout<<"num of item: "<<numOfItem<<std::endl;
    new_select_lex->item_list = newList;

    return new_select_lex;
}

static void
process_table_aliases(const List<TABLE_LIST> &tll, Analysis &a)
{
    RiboldMYSQL::constList_iterator<TABLE_LIST> join_it(tll);
    for (;;) {
        const TABLE_LIST *const t = join_it++;
        if (!t)
            break;

        if (t->is_alias) {
            TEST_TextMessageError(t->db == a.getDatabaseName(),
                                  "Database discrepancry!");
            TEST_TextMessageError(
                a.addAlias(t->alias, t->db, t->table_name),
                "failed to add alias " + std::string(t->alias));
        }

        if (t->nested_join) {
            process_table_aliases(t->nested_join->join_list, a);
            return;
        }
    }
}

static void
process_table_joins_and_derived(const List<TABLE_LIST> &tll,
                                Analysis &a)
{
    RiboldMYSQL::constList_iterator<TABLE_LIST> join_it(tll);
    for (;;) {
        const TABLE_LIST *const t = join_it++;
        if (!t) {
            break;
        }

        if (t->nested_join) {
            process_table_joins_and_derived(t->nested_join->join_list, a);
            return;
        }

        if (t->on_expr) {
            gatherAndAddAnalysisRewritePlan(*t->on_expr, a);
        }

        //std::string db(t->db, t->db_length);
        //std::string table_name(t->table_name, t->table_name_length);
        //std::string alias(t->alias);

        // Handles SUBSELECTs in table clause.
        if (t->derived) {
            // FIXME: Should be const.
            st_select_lex_unit *const u = t->derived;
            // Not quite right, in terms of softness:
            // should really come from the items that eventually
            // reference columns in this derived table.
            process_select_lex(*u->first_select(), a);
        }
    }
}

void
process_table_list(const List<TABLE_LIST> &tll, Analysis & a)
{
    /*
     * later, need to rewrite different joins, e.g.
     * SELECT g2_ChildEntity.g_id, IF(ai0.g_id IS NULL, 1, 0) AS albumsFirst, g2_Item.g_originationTimestamp FROM g2_ChildEntity LEFT JOIN g2_AlbumItem AS ai0 ON g2_ChildEntity.g_id = ai0.g_id INNER JOIN g2_Item ON g2_ChildEntity.g_id = g2_Item.g_id INNER JOIN g2_AccessSubscriberMap ON g2_ChildEntity.g_id = g2_AccessSubscriberMap.g_itemId ...
     */
    
    process_table_aliases(tll, a);
    process_table_joins_and_derived(tll, a);
}

static bool
invalidates(const FieldMeta &fm, const EncSet & es){
    for (const auto &om_it : fm.getChildren()) {
        onion const o = om_it.first.getValue();
        if (es.osl.find(o) == es.osl.end()) {
            return true;
        }
    }

    return false;
}

SIMPLE_UPDATE_TYPE
determineUpdateType(const Item &value_item, const FieldMeta &fm,
                    const EncSet &es)
{
    if (invalidates(fm, es)) {
        return SIMPLE_UPDATE_TYPE::UNSUPPORTED;
    }

    if (value_item.type() == Item::Type::FIELD_ITEM) {
        if (true == isItem_insert_value(value_item)) {
            return SIMPLE_UPDATE_TYPE::ON_DUPLICATE_VALUE;
        } else {
            const std::string &item_field_name =
                static_cast<const Item_field &>(value_item).field_name;
            assert(equalsIgnoreCase(fm.getFieldName(), item_field_name));
            return SIMPLE_UPDATE_TYPE::SAME_VALUE;
        }
    }

    return SIMPLE_UPDATE_TYPE::NEW_VALUE;
}

static void
doPairRewrite(FieldMeta &fm, const EncSet &es,
              const Item_field &field_item, const Item &value_item,
              List<Item> *const res_fields, List<Item> *const res_values,
              Analysis &a)
{
    const std::unique_ptr<RewritePlan> &field_rp =
        constGetAssert(a.rewritePlans,
                       &static_cast<const Item &>(field_item));
    const std::unique_ptr<RewritePlan> &value_rp =
        constGetAssert(a.rewritePlans, &value_item);

    for (auto pair : es.osl) {
        const OLK &olk = {pair.first, pair.second.first, &fm};

        Item *const re_field =
            itemTypes.do_rewrite(field_item, olk, *field_rp, a);
        res_fields->push_back(re_field);

        Item *const re_value =
            itemTypes.do_rewrite(value_item, olk, *value_rp, a);
        res_values->push_back(re_value);
    }
}

static void
addSalt(FieldMeta &fm, const Item_field &field_item,
        List<Item> *const res_fields, List<Item> *const res_values,
        Analysis &a,
        std::function<Item *(const Item_field &rew_fd)> getSaltValue)
{
    assert(res_fields->elements != 0);
    const Item_field * const rew_fd =
        static_cast<Item_field *>(res_fields->head());
    assert(rew_fd);
    const std::string anon_table_name =
        a.getAnonTableName(a.getDatabaseName(), field_item.table_name);
    const std::string anon_field_name = fm.getSaltName();
    res_fields->push_back(make_item_field(*rew_fd,
                                          anon_table_name,
                                          anon_field_name));
    res_values->push_back(getSaltValue(*rew_fd));
}

void
handleUpdateType(SIMPLE_UPDATE_TYPE update_type, const EncSet &es,
                 const Item_field &field_item, const Item &value_item,
                 List<Item> *const res_fields,
                 List<Item> *const res_values, Analysis &a)
{
    FieldMeta &fm =
        a.getFieldMeta(a.getDatabaseName(), field_item.table_name,
                       field_item.field_name);
    switch (update_type) {
        case SIMPLE_UPDATE_TYPE::NEW_VALUE: {
            bool add_salt = false;
            if (fm.getHasSalt()) {
                // Search for a salt first as a previous iteration may 
                // have already referenced this @fm.
                const auto it_salt = a.salts.find(&fm);
                if ((it_salt == a.salts.end()) && needsSalt(es)) {
                    add_salt = true;
                    const salt_type salt = randomValue();
                    a.salts.insert(std::make_pair(&fm, salt));
                }
            }

            doPairRewrite(fm, es, field_item, value_item, res_fields,
                          res_values, a);

            if (add_salt) {
                addSalt(fm, field_item, res_fields, res_values, a,
                        [&fm, &a] (const Item_field &)
                {
                    const salt_type salt = a.salts[&fm];
                    return new (current_thd->mem_root)
                               Item_int(static_cast<ulonglong>(salt));
                });
            }
            break;
        }
        case SIMPLE_UPDATE_TYPE::SAME_VALUE: {
            doPairRewrite(fm, es, field_item, value_item, res_fields,
                          res_values, a);
            break;
        }
        case SIMPLE_UPDATE_TYPE::ON_DUPLICATE_VALUE: {
            doPairRewrite(fm, es, field_item, value_item, res_fields,
                          res_values, a);
            if (fm.getHasSalt()) {
                addSalt(fm, field_item, res_fields, res_values, a,
                        [&value_item, &fm, &a]
                        (const Item_field &rew_fd)
                {
                    const Item_insert_value &insert_value_item =
                       static_cast<const Item_insert_value &>(value_item);
                    const std::string &anon_table_name =
                        rew_fd.table_name;
                    Item_field *const res_field =
                        make_item_field(rew_fd, anon_table_name,
                                        fm.getSaltName());
                    return make_item_insert_value(insert_value_item,
                                                  res_field);
                });
            }
            break;
        }

        case SIMPLE_UPDATE_TYPE::UNSUPPORTED:
        default :
            FAIL_TextMessageError("UNSUPPORTED or unrecognized"
                                  " SIMPLE_UPDATE_TYPE!");
    }

    return;
}

class SetHandler : public DMLHandler {
    virtual void gather(Analysis &a, LEX *const lex) const
    {
    }

    virtual AbstractQueryExecutor *rewrite(Analysis &a, LEX *lex) const
    {
        #define DIRECTIVE_HANDLER(function)                         \
            std::bind((function), this, std::placeholders::_1,      \
                      std::placeholders::_2)
        typedef std::function<AbstractQueryExecutor *(std::map<std::string,
                                                               std::string> &,
                                   Analysis &a)>
            DirectiveHandler;

        static const std::map<std::string, DirectiveHandler> directive_handlers
            {{"show", DIRECTIVE_HANDLER(&SetHandler::handleShowDirective)},
             {"adjust", DIRECTIVE_HANDLER(&SetHandler::handleAdjustDirective)},
             {"sensitive",
              DIRECTIVE_HANDLER(&SetHandler::handleSensitiveDirective)},
             {"killzone",
              DIRECTIVE_HANDLER(&SetHandler::handleKillZoneDirective)}};

        DirectiveHandler dhandler = nullptr;
        std::map<std::string, std::string> var_pairs;
        auto var_it =
            List_iterator<set_var_base>(lex->var_list);
        for (;;) {
            const set_var_base *const v = var_it++;
            if (!v) {
                break;
            }

            switch (v->varType()) {
            case set_var_base::V_USER: {
                const set_var_user *const user_v =
                    static_cast<const set_var_user *>(v);
                Item_func_set_user_var *const i =
                    user_v->*rob<set_var_user, Item_func_set_user_var *,
                                 &set_var_user::user_var_item>::ptr();
                const std::string &var_name = convert_lex_str(i->name);
                const Item *const *const args =
                    i->*rob<Item_func, Item **,
                            &Item_func_set_user_var::args>::ptr();
                assert(args && args[0]);
                // skip non string sets, ie
                //   SET @this = @@that
                if (Item::Type::STRING_ITEM != args[0]->type()) {
                    continue;
                }

                std::string var_value = printItem(*args[0]);
                TEST_TextMessageError(var_value.length() > 2,
                                      "this " + var_value + " is probably"
                                      " not what you meant");
                var_value = var_value.substr(1, var_value.length() - 2);
                if (equalsIgnoreCase("cryptdb", var_name)) {
                    //
                    TEST_TextMessageError(nullptr == dhandler,
                                          "only one directive per query");
                    auto enc = directive_handlers.find(toLowerCase(var_value));
                    TEST_Text(directive_handlers.end() != enc,
                              "unsupported directive: " + var_value);

                    dhandler = enc->second;
                    continue;
                }

                // using the same key twice is not permitted
                TEST_TextMessageError(var_pairs.end()
                                        == var_pairs.find(var_name),
                                      "you double specified: `"
                                      + var_name + "`");

                var_pairs[var_name] = var_value;
            }
            case set_var_base::V_SYSTEM: {
                // do not allow the client to put us into SQL_SAFE_UPDATES
                // mode; else bad things will happen
                const set_var *const set_v =
                    static_cast<const set_var *>(v);
                const sys_var *const sys_v = set_v->var;
                if (NULL == sys_v) {
                    break;
                }

                const std::string &name = convert_lex_str(sys_v->name);
                TEST_Text(false == equalsIgnoreCase("SQL_SAFE_UPDATES", name),
                          "cryptDB does not support SQL_SAFE_UPDATES");
                break;
            }
            case set_var_base::V_PASSWORD:
            case set_var_base::V_COLLATION:
                break;
            default:
                assert(false);
            }
        }

        if (nullptr == dhandler) {
            return new SimpleExecutor();
        }

        return dhandler(var_pairs, a);

        #undef DIRECTIVE_HANDLER
    }

private:
    AbstractQueryExecutor *
    handleAdjustDirective(std::map<std::string, std::string> &var_pairs,
                          Analysis &a) const
    {
        const ParameterCollection &params = collectParameters(var_pairs, a);

        for (const auto &it : params.onions) {
            const OnionMeta &om = a.getOnionMeta(params.fm, it.first);
            const SECLEVEL current_level = a.getOnionLevel(om);
            if (it.second < current_level) {
                throw OnionAdjustExcept(params.tm, params.fm, it.first,
                                        it.second);
            } else if (it.second > current_level) {
                FAIL_TextMessageError("it is not possible to add layers;"
                                      " only remove them");
            }
        }

        return new NoOpExecutor();
    }


    AbstractQueryExecutor *
    handleShowDirective(std::map<std::string, std::string> &var_pairs,
                        Analysis &a) const
    {
        return new ShowDirectiveExecutor(a.getSchema());
    }

    AbstractQueryExecutor *
    handleSensitiveDirective(std::map<std::string, std::string> &var_pairs,
                             Analysis &a) const
    {
        assert(a.deltas.size() == 0);

        const ParameterCollection &params = collectParameters(var_pairs, a);

        for (const auto &it : params.onions) {
            OnionMeta &om = a.getOnionMeta(params.fm, it.first);
            const SECLEVEL current_level = a.getOnionLevel(om);
            if (it.second <= current_level) {
                om.setMinimumSecLevel(it.second);
            } else {
                FAIL_TextMessageError("it is not possible to set a minimum level"
                                      " above the current level!");
            }
            a.deltas.push_back(std::unique_ptr<Delta>(new ReplaceDelta(om,
                                                                     params.fm)));
        }

        return new SensitiveDirectiveExecutor(std::move(a.deltas));
    }

    AbstractQueryExecutor *
    handleKillZoneDirective(std::map<std::string, std::string> &var_pairs,
                        Analysis &a) const
    {
        auto count = var_pairs.find(std::string("count"));
        auto where = var_pairs.find(std::string("where"));
        TEST_Text(var_pairs.end() != count
               && var_pairs.end() != where
               && var_pairs.size() == 2,
                  "the killzone directive takes two parameters, 'count'"
                  " and 'where'");

        KillZone::Where typed_where = typeWhere(where->second);
        try {
            const long long c = std::stoll(count->second.c_str());
            if (c < 0) throw;

            static_assert(sizeof(long long) <= sizeof(uint64_t),
                          "longlong larger than uint64_t");
            a.kill_zone.activate(static_cast<uint64_t>(c), typed_where);
        } catch (...) {
            FAIL_TextMessageError("'count' parameter must be non-negative"
                                  " integer");
        }

        return new NoOpExecutor();
    }

    KillZone::Where
    typeWhere(const std::string &untyped_where) const
    {
        if (equalsIgnoreCase("before", untyped_where)) {
            return KillZone::Where::Before;
        } else if (equalsIgnoreCase("after", untyped_where)) {
            return KillZone::Where::After;
        }

        FAIL_TextMessageError("Unknown 'where' parameter,"
                              " must be 'before' or 'after'");
    }

    struct ParameterCollection {
        ParameterCollection(const Analysis &a,
                            const std::string &database,
                            const std::string &table,
                            const std::string &field,
                            const std::vector<std::pair<onion, SECLEVEL> > onions)
            : database(database), table(table), field(field), onions(onions),
              tm(a.getTableMeta(database, table)),
              fm(a.getFieldMeta(tm, field)) {}
        const std::string database;
        const std::string table;
        const std::string field;
        const std::vector<std::pair<onion, SECLEVEL> > onions;

        TableMeta &tm;
        FieldMeta &fm;
    };

    // destructively modifies var_pairs by removing database, table and field
    ParameterCollection
    collectParameters(std::map<std::string, std::string> &var_pairs,
                      Analysis &a) const
    {
        std::function<std::string(std::string)> getAndDestroy(
            [&var_pairs] (const std::string &key)
        {
            auto it = var_pairs.find(key);
            TEST_TextMessageError(it != var_pairs.end(),
                                  "must supply a " + key);
            const std::string value = it->second;
            var_pairs.erase(it);

            return value;
        });

        const std::string &database = getAndDestroy("database");
        const std::string &table    = getAndDestroy("table");
        const std::string &field    = getAndDestroy("field");

        // the remaining values are <onion>=<level> pairs
        std::vector<std::pair<onion, SECLEVEL> > onions;
        for (auto it = var_pairs.begin(); it != var_pairs.end();) {
            const std::string &str_onion = it->first;
            const std::string &str_level = it->second;

            AssignOnce<onion> o;
            AssignOnce<SECLEVEL> l;
            try {
                o = TypeText<onion>::noCaseToType(str_onion);
                l = TypeText<SECLEVEL>::noCaseToType(str_level);
            } catch (CryptDBError &e) {
                FAIL_TextMessageError("bad param; " + str_onion + "=" +
                                      str_level);
            }

            onions.push_back(std::make_pair(o.get(), l.get()));
            var_pairs.erase(it++);
        }

        TEST_Text(0 == var_pairs.size(),
                  "extraneous directive parameter, indicate a single database,"
                  " table and field; and then onion-seclevel pairs");
        TEST_Text(onions.size() > 0,
                  "you must specify at least one onion");

        return ParameterCollection(a, database, table, field, onions);
    }
};

class ShowTablesHandlers : public DMLHandler {
    virtual void gather(Analysis &a, LEX *const lex) const
    {

    }

    virtual AbstractQueryExecutor *rewrite(Analysis &a, LEX *lex) const
    {
        return new ShowTablesExecutor();
    }
};


//add show create table handler
class ShowCreateTableHandler: public DMLHandler{
    virtual void gather(Analysis &a, LEX *const lex) const
    {

    }
    virtual AbstractQueryExecutor *rewrite(Analysis &a, LEX *lex) const
    {

        int elements = lex->select_lex.table_list.elements;
        assert(elements==1);

        TABLE_LIST *tbl = lex->select_lex.table_list.first;
        std::string db(tbl->db);
        std::string tbn(tbl->table_name);

        TableMeta &tbm = a.getTableMeta(db,tbn);

        //rewrite the table list here        
        LEX *const new_lex = copyWithTHD(lex);
        tbl =  rewrite_table_list(new_lex->select_lex.table_list.first,tbm.getAnonTableName());
        new_lex->select_lex.table_list = *oneElemListWithTHD<TABLE_LIST>(tbl);        

        return new ShowCreateTableExecutor(*new_lex);
    }
};



// FIXME: Add test to make sure handlers added successfully.
SQLDispatcher *buildDMLDispatcher()
{
    DMLHandler *h;
    SQLDispatcher *const dispatcher = new SQLDispatcher();

    h = new InsertHandler();
    dispatcher->addHandler(SQLCOM_INSERT, h);

    h = new InsertHandler();
    dispatcher->addHandler(SQLCOM_REPLACE, h);

    h = new UpdateHandler;
    dispatcher->addHandler(SQLCOM_UPDATE, h);

    h = new DeleteHandler;
    dispatcher->addHandler(SQLCOM_DELETE, h);

    h = new MultiDeleteHandler;
    dispatcher->addHandler(SQLCOM_DELETE_MULTI, h);

    h = new SelectHandler;
    dispatcher->addHandler(SQLCOM_SELECT, h);

    h = new SetHandler;
    dispatcher->addHandler(SQLCOM_SET_OPTION, h);

    h = new ShowTablesHandlers;
    dispatcher->addHandler(SQLCOM_SHOW_TABLES, h);

    //added
    h = new ShowCreateTableHandler;
    dispatcher->addHandler(SQLCOM_SHOW_CREATE,h);


    return dispatcher;
}

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
DMLQueryExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield return CR_QUERY_AGAIN(this->query);
        //crosses init here www.cplusplus.com/forum/beginner/48287/ 
        //errormessage += "DML query failed against remote database";
        TEST_ErrPkt(res.success(), this->query+"DML query failed against remote database");
        yield {
            try {
                return CR_RESULTS(Rewriter::decryptResults(res, this->rmeta));
            } catch (...) {//catch中也有默认参数.
                FAIL_GenericPacketException("error decrypting dml results");
            }
        }
    }

    assert(false);
}

// currently only supports queries that return QUERY_COME_AGAIN
// > this is an attempt to keep this function simple
static std::pair<std::string, ReturnMeta>
rewriteAndGetFirstQuery(const std::string &query, NextParams nparams)
{
    try {
        const std::shared_ptr<const SchemaInfo> schema =
            nparams.ps.getSchemaInfo();
        QueryRewrite delete_rewrite =
            Rewriter::rewrite(query, *schema.get(), nparams.default_db,
                              nparams.ps);

        auto results =
            delete_rewrite.executor->next(ResType(true, 0, 0), nparams);
        assert(AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN
               == results.first);

        return std::make_pair(std::get<1>(results)->
                                extract<std::pair<bool, std::string> >().second,
                              delete_rewrite.rmeta);
    } catch (const SchemaFailure &e) {
        FAIL_GenericPacketException("failed to get schema info");
    } catch (...) {
        FAIL_GenericPacketException("error rewriting a single query");
    }
}

#define SPECIALIZED_SYNC(test)                               \
    SYNC_IF_FALSE((test), nparams.ps.getEConn())

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
SpecialUpdateExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    // FIXME: implement and remove the CALL later on
    /*
    crStartBlock
        const std::string &cond_trx =
            "CALL " + MetaData::Table::conditionalTrx();
        crYield(std::make_pair(true, cond_trx));
    crEndBlock
    */
    reenter(this->corot) {
        assert(res.success());

        yield {
            // Retrieve rows from database.
            const std::string &select_q =
                " SELECT * FROM " + this->plain_table +
                " WHERE " + this->where_clause + ";";
            // Should never cause an onion adjustment
            const auto &rewritten_select_q =
                rewriteAndGetFirstQuery(select_q, nparams);
            this->select_rmeta = rewritten_select_q.second;
            return CR_QUERY_AGAIN(rewritten_select_q.first);
        }
        TEST_ErrPkt(res.success(),
                    "initial select query in SpecialUpdate failed");

        try {
            this->dec_res =
                Rewriter::decryptResults(res, this->select_rmeta.get());
        } catch (...) {
            TEST_ErrPkt(res.success(),
                        "decrypting initial SELECT failed for SpecialUpdate");
        }
        assert(this->dec_res.get().success());
        if (this->dec_res.get().rows.size() == 0) {
            yield return CR_RESULTS(ResType(true, 0, 0));
        }

        yield {
            const auto itemToNiceString =
                [&nparams] (const Item *const p_item)
                {
                    const std::string &s = ItemToString(*p_item);

                    if (Item::Type::STRING_ITEM != p_item->type()) {
                        return s;
                    }

                    // escaping and quoting the string creates a value that can
                    // actually be used in an INSERT statement
                    return "'" + escapeString(nparams.ps.getEConn(), s) + "'";
                };

            // We must take these items and convert them into quoted, escaped
            // strings
            //  > Item -> std::string -> escaped -> quoted
            // then we join the results into a single comma seperated values list
            const auto pItemVectorToNiceValueList =
                [&itemToNiceString]
                    (const std::vector<std::vector<Item *> > &vec)
                {
                    std::vector<std::string> esses;
                    for (auto row_it : vec) {
                        std::vector<std::string> nice_values(row_it.size());
                        std::transform(row_it.begin(), row_it.end(),
                                       nice_values.begin(), itemToNiceString);
                        esses.push_back("("+ vector_join(nice_values, ",") + ")");
                    }

                    return vector_join(esses, ",");
                };

            const std::string &values_string =
                pItemVectorToNiceValueList(dec_res.get().rows);

            // do the query on the embedded database inside of a transaction
            // so that we can prevent failure artifacts from populating the
            // embedded database
            TEST_ErrPkt(nparams.ps.getEConn()->execute("START TRANSACTION;"),
                        "failed to start transaction");

            // turn on strict mode so we can determine if we have bad values
            // > ie trying to insert 256 into a TINYINT UNSIGNED column
            SPECIALIZED_SYNC(strictMode(nparams.ps.getEConn().get()));

            // Push the plaintext rows to the embedded database.
            const std::string &push_q =
                " INSERT INTO " + this->plain_table +
                " VALUES " + values_string + ";";
            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute(push_q));

            // Run the original (unmodified) query on the data in the embedded
            // database.
            std::unique_ptr<DBResult> original_query_dbres;
            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute(
                                nparams.original_query, &original_query_dbres));
            assert(original_query_dbres);
            // HACK
            this->original_query_dbres = original_query_dbres.release();

            // strict mode off
            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute(
                                "SET SESSION sql_mode = ''"));

            // > Collect the results from the embedded database.
            // > This code relies on single threaded access to the database
            //   and on the fact that the database is cleaned up after
            //   every such operation.
            std::unique_ptr<DBResult> dbres;
            const std::string &select_results_q =
                " SELECT * FROM " + this->plain_table + ";";
            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute(select_results_q,
                                                            &dbres));
            const ResType interim_res = ResType(dbres->unpack());
            assert(interim_res.success());
            this->escaped_output_values =
                pItemVectorToNiceValueList(interim_res.rows);

            // Cleanup the embedded database.
            const std::string &cleanup_q =
                "DELETE FROM " + this->plain_table + ";";
            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute(cleanup_q));

            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute("COMMIT;"));

            // This query is necessary to propagate a transaction into
            // INFORMATION_SCHEMA.
            return CR_QUERY_AGAIN(
                "SELECT NULL FROM " + this->crypted_table + ";");
        }

        TEST_ErrPkt(res.success(),
            "transaction propagation query failed in SpecialUpdate");

        yield return CR_QUERY_AGAIN(
            "CALL " + MetaData::Proc::activeTransactionP());
        TEST_ErrPkt(res.success(),
                    "failed to determine if we are in a transaction");
        this->in_trx = handleActiveTransactionPResults(res);

        if (false == this->in_trx.get()) {
            yield return CR_QUERY_AGAIN("START TRANSACTION");
            TEST_ErrPkt(res.success(),
                        "failed to start transaction in SpecialUpdate");
        }

        yield {
            // DELETE the rows matching the WHERE clause from the database.
            const std::string &delete_q =
                " DELETE FROM " + this->plain_table +
                " WHERE " + this->where_clause + ";";
            const auto &rewritten_delete_q =
                rewriteAndGetFirstQuery(delete_q, nparams);
            return CR_QUERY_AGAIN(rewritten_delete_q.first);
        }
        CR_ROLLBACK_AND_FAIL(res, "delete query failed in SpecialUpdate");

        yield {
            // > Add each row from the embedded database to the data database.
            const std::string &insert_q =
                " INSERT INTO " + this->plain_table +
                " VALUES " + this->escaped_output_values.get() + ";";
            const auto &rewritten_insert_q =
                rewriteAndGetFirstQuery(insert_q, nparams);
            return CR_QUERY_AGAIN(rewritten_insert_q.first);
        }
        CR_ROLLBACK_AND_FAIL(res, "insert query failed in SpecialUpdate");

        if (false == this->in_trx.get()) {
            yield return CR_QUERY_AGAIN("COMMIT");
            CR_ROLLBACK_AND_FAIL(res, "commit failed in SpecialUpdate");
        }

        /*
        crStartBlock
            const std::string &cond_trx =
                "CALL " + MetaData::Table::conditionalCommit();
            crYield(std::make_pair(true, cond_trx));
        crEndBlock
        */

        return CR_RESULTS(this->original_query_dbres.get()->unpack());
    }

    assert(false);
}

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
ShowDirectiveExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield {
            TEST_ErrPkt(deleteAllShowDirectiveEntries(nparams.ps.getEConn()),
                        "failed to initialize show directives table");

            // HACK hack HACK hackity hackhack
            const auto &databases = this->schema.getChildren();
            for (const auto &db_it : databases) {
                const std::string &db_name = db_it.first.getValue();
                const auto &dm = db_it.second;
                const auto &tables = dm->getChildren();
                for (const auto &table_it : tables) {
                    const std::string &table_name = table_it.first.getValue();
                    const auto &tm = table_it.second;
                    const auto &fields = tm->getChildren();
                    for (const auto &field_it : fields) {
                        const std::string &field_name =
                            field_it.first.getValue();
                        const auto &fm = field_it.second;
                        const auto &onions = fm->getChildren();
                        for (const auto &onion_it : onions) {
                            const std::string &onion_name =
                              TypeText<onion>::toText(onion_it.first.getValue());
                            const auto &om = onion_it.second;

                            // HACK: this behavior is not usually safe, use
                            // Analysis to get state information generally
                            const std::string &level =
                                TypeText<SECLEVEL>::toText(om->getSecLevel());
                            const bool b =
                                addShowDirectiveEntry(nparams.ps.getEConn(),
                                                      db_name, table_name,
                                                      field_name, onion_name,
                                                      level);
                            TEST_ErrPkt(true == b,
                                        "failed producing directive results");
                        }
                    }
                }
            }

            std::unique_ptr<DBResult> db_res;
            TEST_ErrPkt(getAllShowDirectiveEntries(nparams.ps.getEConn(),
                                                   &db_res),
                        "failed retrieving directive results");
            return CR_RESULTS(db_res->unpack());
        }
    }

    assert(false);
}

bool ShowDirectiveExecutor::
deleteAllShowDirectiveEntries(const std::unique_ptr<Connect> &e_conn)
{
    const std::string &query =
        "DELETE FROM " + MetaData::Table::showDirective() + ";";
    return e_conn->execute(query);
}

bool ShowDirectiveExecutor::
addShowDirectiveEntry(const std::unique_ptr<Connect> &e_conn,
                      const std::string &database,
                      const std::string &table,
                      const std::string &field,
                      const std::string &onion,
                      const std::string &level)
{
    const std::string &query =
        "INSERT INTO " + MetaData::Table::showDirective() +
        " (_database, _table, _field, _onion, _level) VALUES "
        " ('" + database + "', '" + table + "',"
        "  '" + field + "', '" + onion + "', '" + level + "')";

    return e_conn->execute(query);
}

bool ShowDirectiveExecutor::
getAllShowDirectiveEntries(const std::unique_ptr<Connect> &e_conn,
                           std::unique_ptr<DBResult> *db_res)
{
    assert(db_res);
    const std::string &query =
        "SELECT * FROM " + MetaData::Table::showDirective() + ";";
    return e_conn->execute(query, db_res);
}

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
SensitiveDirectiveExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield {
            TEST_ErrPkt(nparams.ps.getEConn()->execute("START TRANSACTION"),
                      "failed to start transaction for sensitive directive");

            SPECIALIZED_SYNC(writeDeltas(nparams.ps.getEConn(), this->deltas,
                                         Delta::BLEEDING_TABLE));

            SPECIALIZED_SYNC(writeDeltas(nparams.ps.getEConn(), this->deltas,
                                         Delta::REGULAR_TABLE));

            SPECIALIZED_SYNC(nparams.ps.getEConn()->execute("COMMIT"));

            return CR_QUERY_RESULTS("DO 0;");
        }
    }

    assert(false);
}

#undef SPECIALIZED_SYNC

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
ShowTablesExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield return CR_QUERY_AGAIN(nparams.original_query);
        TEST_ErrPkt(res.success(), "show tables failed");

        yield {
            const std::shared_ptr<const SchemaInfo> &schema =
                nparams.ps.getSchemaInfo();
            const DatabaseMeta *const dm =
                schema->getChild(IdentityMetaKey(nparams.default_db));
            TEST_ErrPkt(dm, "failed to find the database '"
                            + nparams.default_db + "'");
            std::vector<std::vector<Item *> > new_rows;

            for (const auto &it : res.rows) {
                assert(1 == it.size());
                for (const auto &table : dm->getChildren()) {    
                    assert(table.second);
                    if (table.second->getAnonTableName()
                        == ItemToString(*it.front())) {

                        const IdentityMetaKey &plain_table_name
                            = dm->getKey(*table.second.get());
                        new_rows.push_back(std::vector<Item *>
                            {make_item_string(plain_table_name.getValue())});
                    }
                }
            }
            return CR_RESULTS(ResType(res, new_rows));
        }
    }
    assert(false);
}

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
ShowCreateTableExecutor::
nextImpl(const ResType &res, const NextParams &nparams){
    //return CR_QUERY_AGAIN(nparams.original_query);
    reenter(this->corot) {
        yield return CR_QUERY_AGAIN(this->query);
        TEST_ErrPkt(res.success(), "show create table tables failed");
        yield {
            const std::shared_ptr<const SchemaInfo> &schema =
                nparams.ps.getSchemaInfo();
            const DatabaseMeta *const dm =
                schema->getChild(IdentityMetaKey(nparams.default_db));
            TEST_ErrPkt(dm, "failed to find the database '"
                            + nparams.default_db + "'");
            std::vector<std::vector<Item *> > new_rows;
            return CR_RESULTS(ResType(res, new_rows));
        }
    }
    assert(false);
}
