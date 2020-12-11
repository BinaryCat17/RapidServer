#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include "sqliteUtils.hpp"
#include "traits.hpp"

namespace rapid {
    namespace impl {
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

        struct File {
            int id;
            string path;
        };

        struct FileAccessGroup {
            int fileId;
            int groupId;
        };

        struct FileAccessUser {
            int fileId;
            int userId;
        };

        struct Session {
            int id;
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
                make_table("File",
                    make_column("Id", &File::id, autoincrement(), primary_key()),
                    make_column("Path", &File::path)),
                make_table("FileAccessGroup",
                    make_column("File", &FileAccessGroup::fileId),
                    make_column("GroupId", &FileAccessGroup::groupId)),
                make_table("FileAccessUser",
                    make_column("File", &FileAccessUser::fileId),
                    make_column("UserId", &FileAccessUser::userId)),
                make_table("Session",
                    make_column("Id", &Session::id),
                    make_column("UserId", &Session::userId))
            );

            return s;
        }

        using namespace db;

        using Storage = decay_t<decltype(storage("", ""))>;

        using Socket = uWS::WebSocket<false, true> *;

        template<typename SocketT, typename StorageT>
        struct Connection {
            SocketT socket;
            StorageT storage;
        };

        struct UserData {
            int id = -1;
        };
    }

    namespace server {
        template<>
        struct SignControl<impl::Storage> {
            using session_type = int;

            static optional<session_type>
            signIn(impl::Storage &s, string const &name, string const &pass) {
                if (auto user = getByName<impl::User>(s, name)) {
                    if (user->password == pass) {
                        impl::Session session {.id = -1, .userId = user->id};
                        return insertTo(s, session);
                    }
                } else {
                    return {};
                }
            }

            static void signOut(impl::Storage &e, session_type session) {
                e.remove<impl::Session>(session);
            }
        };

        template<>
        struct FileAccessControl<impl::Storage> {
            using user_type = int;

            using group_type = int;

            using file_type = int;

            static bool canReadFile(impl::Storage &storage, user_type user, file_type file) {
                using namespace impl;
                auto groups = select(&UserGroup::groupId,
                    where(is_equal(&UserGroup::userId, user)));
                auto groupFile = in(&FileAccessGroup::groupId, groups) and
                    is_equal(&FileAccessGroup::fileId, file);
                auto userFile = in(&FileAccessUser::userId, user) and
                    is_equal(&FileAccessUser::fileId, file);
                return isExists(storage, groupFile or userFile);
            }

            static void giveUserAccessFile(impl::Storage &storage, user_type user, file_type file) {
                storage.insert(impl::FileAccessUser {.fileId = file, .userId = user});
            }

            static void giveGroupAccessFile(impl::Storage &storage, group_type group, file_type file) {
                storage.insert(impl::FileAccessGroup {.fileId = file, .groupId = group});
            }
        };

        template<>
        struct UserControl<impl::Storage> {
            using user_type = int;

            static optional<user_type> newUser(impl::Storage &storage, string name, string password) {
                using namespace db;

                if (isExists(storage, is_equal(&impl::User::name, name))) {
                    return {};
                }

                return {storage.insert(impl::User {-1, move(name), move(password)})};
            }

            static optional<user_type>
            signIn(impl::Storage &s, string const &name, string const &password) {
                using namespace db;

                if (auto user = getByName<impl::User>(s, name)) {
                    if (user->password == password) {
                        return user->id;
                    }
                } else {
                    return {};
                }
            }
        };


        template<>
        struct ServerResponse<impl::Socket> {
            // newUser success (session)
            static void newUser(impl::Socket socket, Success <string_view> session) {
                socket->send("newUser success " + string(session.v));
            }

            // newUser error(message)
            static void newUser(impl::Socket socket, Error <string_view> error) {
                socket->send("newUser error " + string(error.v));
            }

            // signIn success (session)
            static void signIn(impl::Socket socket, Success <string_view> session) {
                socket->send("signIn success " + string(session.v));
            }

            // signIn error (message)
            static void signIn(impl::Socket socket, Error <string_view> error) {
                socket->send("signIn error " + string(error.v));
            }

            // signOut success
            static void signOut(impl::Socket socket) {
                socket->send("signOut success");
            }

            // connectFarm success (session)
            static void connectFarm(impl::Socket socket, Success <string_view> session) {
                socket->send("connectFarm success " + string(session.v));
            }

            // connectFarm error (message)
            static void connectFarm(impl::Socket socket, Error <string_view> session) {
                socket->send("connectFarm error " + string(session.v));
            }
        };

        template<typename UserDataT>
        struct UserDataControl<impl::Socket, UserDataT> {
            static UserDataT const* getUserData(impl::Socket socket) {
                return reinterpret_cast<UserDataT const*>(socket->getUserData());
            }

            static UserDataT* getMutUserData(impl::Socket socket) {
                return reinterpret_cast<UserDataT*>(socket->getUserData());
            }
        };

        template<typename SocketT, typename StorageT>
        struct ServerRequest<impl::Connection<SocketT, StorageT>> {
            using Response = ServerResponse<SocketT>;
            using Storage = UserControl<StorageT>;
            using Data = UserDataControl<SocketT, impl::UserData>;
            using SignControl = SignControl<StorageT>;
            using Connection = impl::Connection<SocketT, StorageT>;

            // newUser (string, string)
            void newUser(Connection& connection, string_view login, string_view password) {
                if(auto user = Storage::newUser(connection.storage, login, password)) {
                    *Data::getMutUserData(connection.socket) = *user;
                } else {
                    Response::newUser(Error<string_view>{"User already exist!"});
                }
            }

            // signIn (string, string)
            void signIn(Connection& connection, string_view login, string_view password) {
                auto data = *Data::getMutUserData(connection.socket);
                if(*data == -1) {
                    Response::signIn(Error<string_view>{"Already signed in!"});
                    return;
                }
                if(auto user = SignControl::signIn(connection.storage, login, password)) {
                    *Data::getMutUserData(connection.socket) = *user;
                } else {
                    Response::signIn(Error<string_view>{"Incorrect login or password"});
                }
            }

            // signOut
            void signOut(Connection& connection) {
                SignControl::signOut(Data::getUserData(connection.socket)->id);
            }
        };
    }

    namespace farm {
        template<>
        struct ServerResponse<impl::Socket> {
            // connectFarm success (session)
            static void connectFarm(impl::Socket socket, Success<string_view> session) {
                socket->send("connectFarm success " + string(session.v));
            }

            // connectFarm error (message)
            static void connectFarm(impl::Socket socket, Error<string_view> session) {
                socket->send("connectFarm error " + string(session.v));
            }
        };

        template<>
        struct ServerRequest<impl::Socket> {
            // connectFarm (string)
            void connectFarm(string_view farm, string_view farmPassword);
        };
    }
}