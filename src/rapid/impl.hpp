#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include "sqliteUtils.hpp"
#include "traits.hpp"
#include <map>
#include <concepts>
#include <uWebSockets/App.h>

namespace rapid {
    namespace impl {
        namespace fs = filesystem;

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

        struct CacheExplorer {
            fs::path mRoot;
            std::map<fs::path, string> mCache;
        };

        using Socket = uWS::WebSocket<false, true> *;

        using Response = uWS::HttpResponse<false> *;
        using Request = uWS::HttpRequest *;
        using UserData = optional<int>;


    }

    namespace storage {
        template<typename... T>
        requires implements_v<Default, T...>
        auto make(auto &&, Traits<T...>, filesystem::path const &root, string_view filename) {
            using namespace db;
            using namespace impl;

            fs::create_directory(root);

            auto s = make_storage(string(filename),
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
    }

    namespace impl {
        using Storage = decay_t<decltype(storage::make(storage::Default {},
            Traits<storage::Default> {}, "", ""))> *;
    }

    namespace sign_control {
        template<typename... T>
        optional<int> sign_in(take_ref<impl::Storage> auto &&v, Traits<T...>, string_view name,
            string_view pass) {
            auto &s = static_cast<impl::Storage &>(v);
            if (auto user = getByName<impl::User>(s, string(name))) {
                if (user->password == pass) {
                    impl::Session session {.id = -1, .userId = user->id};
                    return insertTo(s, session);
                }
            } else {
                return {};
            }
        }

        template<typename... T>
        void sign_out(take_ref<impl::Storage> auto &&v, Traits<T...>, int session) {
            auto &s = static_cast<impl::Storage &>(v);
            s->remove<impl::Session>(session);
        }
    }

    namespace file_access {
        template<typename... T>
        bool can_read_file(take_ref<impl::Storage> auto &&v, Traits<T...>, int user, int file) {
            auto &s = static_cast<impl::Storage &>(v);
            using namespace impl;
            using namespace db;

            auto groups = select(&UserGroup::groupId,
                where(is_equal(&UserGroup::userId, user)));
            auto groupFile = in(&FileAccessGroup::groupId, groups) and
                is_equal(&FileAccessGroup::fileId, file);
            auto userFile = in(&FileAccessUser::userId, user) and
                is_equal(&FileAccessUser::fileId, file);
            return isExists(*s, groupFile or userFile);
        }

        template<typename... T>
        void
        give_user_access_file(take_ref<impl::Storage> auto &&v, Traits<T...>, int user, int file) {
            auto &s = static_cast<impl::Storage &>(v);
            s->insert(impl::FileAccessUser {.fileId = file, .userId = user});
        }

        template<typename... T>
        void give_group_access_file(take_ref<impl::Storage> auto &&v, Traits<T...>, int group,
            int file) {
            auto &s = static_cast<impl::Storage &>(v);
            s->insert(impl::FileAccessGroup {.fileId = file, .groupId = group});
        }
    }

    namespace user_control {
        template<typename... T>
        optional<int> new_user(take_ref<impl::Storage> auto &&v, Traits<T...>, string_view name,
            string_view password) {
            auto &s = static_cast<impl::Storage &>(v);
            using namespace db;

            if (isExists(*s, is_equal(&impl::User::name, string(name)))) {
                return {};
            }

            return {s->insert(impl::User {-1, string(name), string(password)})};
        }

        template<typename... T>
        optional<int>
        add_user_group(take_ref<impl::Storage> auto &&v, Traits<T...>, string_view name,
            string_view password) {
            auto &s = static_cast<impl::Storage &>(v);
            using namespace db;

            if (auto user = getByName<impl::User>(s, string(name))) {
                if (user->password == password) {
                    return user->id;
                }
            } else {
                return {};
            }
        }
    }

    namespace user_data {
        template<typename... T>
        impl::UserData const *get_user_data(take_ref<impl::Socket> auto &&v, Traits<T...>) {
            auto &s = static_cast<impl::Socket const &>(v);
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        template<typename... T>
        impl::UserData *get_mut_user_data(take_ref<impl::Socket> auto &&v, Traits<T...>) {
            auto &s = static_cast<impl::Socket const &>(v);
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }
    }

    namespace explorer {
        template<typename... T>
        requires implements_v<Default, T...>
        auto make(auto &&, Traits<T...>, filesystem::path const &root) {
            return impl::CacheExplorer {.mRoot = root};
        }

        template<typename... T>
        string_view file(take_ref<impl::CacheExplorer> auto &&v, Traits<T...>, fs::path const &f) {
            auto &s = static_cast<impl::CacheExplorer &>(v);
            return atOrInsert(s.mCache, f, [&]{ return readFile(s.mRoot / f); });
        }
    }

    namespace response {
        template<typename... T>
        requires implements_v<Default, T...>
        void write(take_ref<impl::Response> auto &&v, Traits<T...>, string_view message) {
            auto &s = static_cast<impl::Response const &>(v);
            s->write(message);
        }
    }

    namespace request {
        template<typename... T>
        requires implements_v<Default, T...>
        string_view url(take_ref<impl::Request> auto &&v, Traits<T...>) {
            auto &s = static_cast<impl::Request const &>(v);
            return s->getUrl();
        }
    }

    namespace server_callback {
        template<typename... T>
        requires implements_v<Default, T...>
        void get(auto &&e, Traits<T...> traits) {
            try {
                response::write(e, traits, explorer::file(e, traits, "RapidControl.html"));
            } catch (exception const &err) {
                cerr << err.what() << endl;
                response::write(e, traits, err.what());
            }
        }
    }

    namespace socket {
        template<typename... TT>
        auto const *get_user_data(take_ref<impl::Socket> auto &&v, Traits<TT...>) {
            auto &s = static_cast<impl::Socket const &>(v);
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        template<typename... TT>
        auto *get_mut_user_data(take_ref<impl::Socket> auto &&v, Traits<TT...>) {
            auto &s = static_cast<impl::Socket const &>(v);
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }

        template<typename... TT>
        void send(take_ref<impl::Socket> auto &&v, Traits<TT...>, string_view message) {
            auto &s = static_cast<impl::Socket const &>(v);
            s->send(message);
        }

        template<typename... TT>
        string address(take_ref<impl::Socket> auto &&v, Traits<TT...>) {
            auto &s = static_cast<impl::Socket const &>(v);
            return string(s->getRemoteAddressAsText());
        }
    }


    namespace impl {
        template<typename T, typename... TT>
        void systemError(T &&e, Traits<TT...> traits, string_view msg) {
            socket::send(e, traits, "error: " + string(msg));
            cerr << "error: " << msg << endl;
        }
    }

    namespace socket_callback {
        template<typename... T>
        requires implements_v<Default, T...>
        void open(auto &&e, Traits<T...> traits) {
            cout << "New connection! " << socket::address(e, traits) << endl;
        }

        template<typename T, typename... TT>
        requires implements_v<Default, TT...>
        void message(T &&e, Traits<TT...> traits, string_view message) {
            istringstream iss(string {message});
            string command;
            iss >> command;

            using Command = function<void(istream &)>;

            static std::map<string, Command> commands {
                {"new_user", stream([xe = FWD_CAPTURE(e), traits](string_view m, string_view p){server_request::new_user(xe, traits, m, p);})},
                {"sign_in", stream([xe = FWD_CAPTURE(e), traits](string_view m, string_view p){server_request::sign_in(xe, traits, m, p);})},
                {"sign_out", stream([xe = FWD_CAPTURE(e), traits](){server_request::sign_out(xe, traits);})},
            };

            try {
                commands.at(command)(iss);
            } catch (exception const &) {
                impl::systemError(e, traits, "error: Cannot parse command - " + command);
            }
        }
    }

    namespace server {
        template<typename... T>
        requires implements_v<Default, T...>
        auto start(take_ref<uWS::App> auto &&v, Traits<T...> traits, unsigned port) {
            auto &s = static_cast<uWS::App &>(v);
            using UserData = decay_t<decltype(socket::get_user_data(impl::Socket {}, traits))>;

            s.get("/*", [&s, traits](impl::Response res, impl::Request req) {
                server_callback::get(Merge(s, res, req), traits);
            }).template ws<UserData>("/*", uWS::App::WebSocketBehavior {
                .open = [&s, traits](impl::Socket ws) {
                    socket_callback::open(Merge(s, ws), traits);
                },
                .message = [&s, traits](impl::Socket ws, string_view message, uWS::OpCode) {
                    socket_callback::message(Merge(s, ws), traits, message);
                }
            }).listen("0.0.0.0", port, [](auto *listenSocket) {
                if (listenSocket) {
                    cout << "Listening for connections..." << endl;
                }
            }).run();
        }
    }
}
