#pragma once
#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include <optional>

namespace rapid {
    namespace db = sqlite_orm;

    template <typename Storage, typename Query>
    bool isExists(Storage && s, Query const& q) {
         auto v = s.prepare(db::select(1, db::where(db::exists(db::select(1, db::where(q))))));
         cout << "SQL: " << v.sql() << endl;
         return false;
    }

    template<typename Arg, typename Storage>
    auto insertTo(Storage s, Arg & arg) {
        auto id = s->insert(arg);
        arg.id = id;
        return id;
    }

    template<typename Arg, typename Storage>
    auto getByName(Storage s, std::string const& name) {
        auto a = s->template get_all<Arg>(db::where(db::is_equal(&Arg::name, name)),
            db::limit(1));
        if(a.empty()) {
            return std::optional<Arg>();
        } else {
            return std::optional<Arg>(a.front());
        }
    }

}