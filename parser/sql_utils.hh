#pragma once

#include <util/util.hh>
#include <vector>
#include <string>
#include <memory>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>


// must be called before we can use any MySQL AP
void
init_mysql(const std::string & embed_db);

class ResType {
public:
    const bool ok;  // query executed successfully
    const uint64_t affected_rows;
    const uint64_t insert_id;
    const std::vector<std::string> names;
    const std::vector<enum_field_types> types;
    const std::vector<std::vector<Item *> > rows;

    ResType(bool okflag, uint64_t affected_rows, uint64_t insert_id,
            const std::vector<std::string> &&names = std::vector<std::string>(),
            std::vector<enum_field_types> &&types =
                std::vector<enum_field_types>(),
            std::vector<std::vector<Item *> > &&rows =
                std::vector<std::vector<Item *> >())
        : ok(okflag), affected_rows(affected_rows),
          insert_id(std::move(insert_id)), names(std::move(names)),
          types(std::move(types)), rows(rows) {}

    ResType(const ResType &res, const std::vector<std::vector<Item *> > &rows)
        : ok(res.ok), affected_rows(res.affected_rows), insert_id(res.insert_id),
          names(res.names), types(res.types), rows(rows) {}

    bool success() const {return this->ok;}
};

char * make_thd_string(const std::string &s, size_t *lenp = 0);

std::string ItemToString(const Item &i);
std::string printItemToString(const Item &i);

