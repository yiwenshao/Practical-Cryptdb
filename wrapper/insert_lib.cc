#include "wrapper/insert_lib.hh"

Item *
my_encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    const Analysis &a, uint64_t IV) {
//    assert(!RiboldMYSQL::is_null(i));
    if(RiboldMYSQL::is_null(i)) {
        return RiboldMYSQL::clone_item(i);
    }
    const auto &enc_layers = a.getEncLayers(om);
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    const Item *enc = &i;
    Item *new_enc = NULL;
    for (const auto &it : enc_layers) {
        new_enc = it->encrypt(*enc, IV);
        assert(new_enc);
        enc = new_enc;
    }
    assert(new_enc && new_enc != &i);
    return new_enc;
}


std::ostream&
simple_insert(std::ostream &out, LEX &lex){
        String s;
        THD *t = current_thd;
        const char* cmd = "INSERT";
        out<<cmd<<" ";
        lex.select_lex.table_list.first->print(t, &s, QT_ORDINARY);
        out << "INTO " << s;
        out << " values " << noparen(lex.many_values);
        return out;
}


std::string
convert_insert(const LEX &lex)
{
    std::ostringstream o;
    simple_insert(o,const_cast<LEX &>(lex));
    return o.str();
}


void
my_typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                            Analysis &a, std::vector<Item *> *l) {
    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;
    uint64_t IV = salt;
    for (auto it : fm.orderedOnionMetas()) {
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        l->push_back(my_encrypt_item_layers(i, o, *om, a, IV));
    }
    if (fm.getHasSalt()) {
        l->push_back(new Item_int(static_cast<ulonglong>(salt)));
    }
}


void myRewriteInsertHelper(const Item &i, const FieldMeta &fm, Analysis &a,
                           List<Item> *const append_list){
    std::vector<Item *> l;
    my_typical_rewrite_insert_type(i,fm,a,&l);
    for (auto it : l) {
        append_list->push_back(it);
    }
}



