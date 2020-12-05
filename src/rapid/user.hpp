#pragma once
#include "utils.hpp"
#include "file.hpp"
#include <sqlite_orm/sqlite_orm.h>

namespace rapid {
    using namespace sqlite_orm;

    struct User {
        int id;
        string name;
        vector<int> groups;
        string password;
    };

    struct Group {
        int id;
        string name;
        vector<fs::path> accessFiles;
    };

    auto storage(fs::path const& filename) {
        return make_storage("db.sqlite",
            make_table("users",
                make_column("id", &User::id, autoincrement(), primary_key()),
                make_column("name", &User::name),
                make_column("groups", &User::groups),
                make_column("password", &User::password),
            make_table("groups",
                make_column("id", &Group::id, autoincrement(), primary_key()),
                make_column("name", &Group::name),
                make_column("accessFiles", &Group::accessFiles))));
    }
    using Storage = decay_t<decltype(storage(""))>;

    template<typename T>
    struct Access {
        static bool canReadFile(T && e, string_view username, fs::path const& file);
    };

    template<>
    struct Access<Storage> {
        static bool canReadFile(Storage const& storage, string_view username, fs::path const& file) {
            auto selectStatement = storage.prepare(select(&User::groups, where(length(&Visit::patient_name) > 8)));
            storage.get
        }
    };
}