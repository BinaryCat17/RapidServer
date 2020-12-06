#pragma once
#include "utils.hpp"
#include "file.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include "sqliteUtils.hpp"

namespace rapid {

    struct User {
        int id;
        string name;
        string password;
    };

    struct Group {
        int id;
        string name;
    };

    struct UserGroup {
        int userId;
        int groupId;
    };

    struct FileAccessGroup {
        string file;
        int groupId;
    };

    struct FileAccessUser {
        string file;
        int userId;
    };

    auto storage(fs::path const &root, string const &filename) {
        using namespace db;

        fs::create_directory(root);

        auto s = make_storage(filename,
            make_table("Users",
                make_column("Id", &User::id, autoincrement(), primary_key()),
                make_column("Name", &User::name, unique()),
                make_column("Password", &User::password)),
            make_table("Groups",
                make_column("Id", &Group::id, autoincrement(), primary_key()),
                make_column("Name", &Group::name, unique())),
            make_table("UserGroup",
                make_column("UserId", &UserGroup::userId),
                make_column("GroupId", &UserGroup::groupId)),
            make_table("FileAccessGroup",
                make_column("File", &FileAccessGroup::file),
                make_column("GroupId", &FileAccessGroup::groupId)),
            make_table("FileAccessUser",
                make_column("File", &FileAccessUser::file),
                make_column("UserId", &FileAccessUser::userId))
        );

        return s;
    }

    using Storage = decay_t<decltype(storage("", ""))>;

    template<typename S>
    struct Access {
        using user_type = nullptr_t;

        using group_type = nullptr_t;

        static bool canReadFile(S &&e, typename S::user_type const &user, fs::path const &file);

        static void accessUserFiles(S &&e, user_type user, vector<fs::path> const& files);

        static void accessGroupFiles(S &&e, group_type group, vector<fs::path> const& files);
    };

    template<>
    struct Access<Storage> {
        using user_type = int;

        static bool canReadFile(Storage &storage, user_type user, fs::path const &file) {
            using namespace db;
            auto groups = select(&UserGroup::groupId, where(is_equal(&UserGroup::userId, user)));
            auto groupFiles = in(&FileAccessGroup::groupId, groups) and
                is_equal(&FileAccessGroup::file, file.string());
            auto userFiles = in(&FileAccessUser::userId, user) and
                is_equal(&FileAccessUser::file, file.string());
            return isExists(storage, groupFiles and userFiles);
        }
    };

    template<typename S>
    struct Accounts {
        using user_type = std::nullptr_t;

        using group_type = std::nullptr_t;

        static  optional<user_type>
        newUser(S &&e, string_view name, string_view password);

        static optional<user_type>
        signIn(S &&e, string_view name, string_view password);

        static void setGroups(S &&e, user_type user, vector<group_type> const& groups);


    };

    template<>
    struct Accounts<Storage> {
        using user_type = int;

        static optional<user_type> newUser(Storage &storage, string name, string password) {
            using namespace db;

            if (isExists(storage, is_equal(&User::name, name))) {
                return {};
            }

            return {storage.insert(User {-1, move(name), move(password)})};
        }

        static optional<user_type>
        signIn(Storage &storage, string const &name, string const &password) {
            using namespace db;

            if (auto user = getByName<User>(storage, name)) {
                if (user->password == password) {
                    return user.id;
                }
            } else {
                return {};
            }
        }
    };
}