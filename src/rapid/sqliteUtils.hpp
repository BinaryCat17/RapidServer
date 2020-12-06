#pragma once
#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include <optional>

namespace rapid {
    namespace db = sqlite_orm;

    template <typename Storage, typename Query>
    bool isExists(Storage && s, Query const& q) {
        return s.select(1, db::where(db::exists(db::select(db::columns(), db::where(q))))).front();
    }

    template<typename Arg, typename Storage>
    auto insertTo(Storage& s, Arg & arg) {
        auto id = s.insert(arg);
        arg.id = id;
        return id;
    }

    template<typename Arg, typename Storage>
    auto getByName(Storage& s, string const& name) {
        auto a = s.template get_all<Arg>(db::where(db::is_equal(&Arg::name, name)));
        if(a.empty()) {
            return optional<Arg>();
        } else {
            return optional<Arg>(a.front());
        }
    }

}