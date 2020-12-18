#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include "sqliteUtils.hpp"
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
        auto make(filesystem::path const &root, string_view filename) {
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
        using Storage = decay_t<decltype(storage::make("", ""))> *;
    }

    namespace sign_control {
        optional<int> sign_in(impl::Storage s, string_view name, string_view pass) {
            if (auto user = getByName<impl::User>(s, string(name))) {
                if (user->password == pass) {
                    impl::Session session {.id = -1, .userId = user->id};
                    return insertTo(s, session);
                }
            } else {
                return {};
            }
        }

        void sign_out(impl::Storage s, int session) {
            s->remove<impl::Session>(session);
        }
    }

    namespace file_access {
        bool can_read_file(impl::Storage s, int user, int file) {
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

        void give_user_access_file(impl::Storage s, int user, int file) {
            s->insert(impl::FileAccessUser {.fileId = file, .userId = user});
        }

        void give_group_access_file(impl::Storage s, int group, int file) {
            s->insert(impl::FileAccessGroup {.fileId = file, .groupId = group});
        }
    }

    namespace user_control {
        optional<int> new_user(impl::Storage s, Traits<T...>, string_view name, string_view pass) {
            using namespace db;

            if (isExists(*s, is_equal(&impl::User::name, string(name)))) {
                return {};
            }

            return {s->insert(impl::User {-1, string(name), string(pass)})};
        }

        optional<int> add_user_group(impl::Storage s, string_view name, string_view password) {
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
        impl::UserData const *get_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        template<typename... T>
        impl::UserData *get_mut_user_data(impl::Socket vs {
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }
    }

    namespace explorer {
        auto make(filesystem::path const &root) {
            return impl::CacheExplorer {.mRoot = root};
        }

        string_view file(impl::CacheExplorer &s, fs::path const &f) {
            return atOrInsert(s.mCache, f, [&]{ return readFile(s.mRoot / f); });
        }
    }

    namespace response {
        void write(impl::Response s, string_view message) {
            s->write(message);
        }
    }

    namespace request {
        string_view url(impl::Request s) {
            return s->getUrl();
        }
    }

    namespace server_callback {
        void get(auto &&e) {
            try {
                response::write(e, explorer::file(e, "RapidControl.html"));
            } catch (exception const &err) {
                cerr << err.what() << endl;
                response::write(e, err.what());
            }
        }
    }

    namespace socket {
        auto const *get_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        auto *get_mut_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }

        void send(impl::Socket s, string_view message) {
            s->send(message);
        }

        string address(impl::Socket s, Traits<TT...>) {
            return string(s->getRemoteAddressAsText());
        }
    }

    namespace impl {
        template<typename T>
        void systemError(T &&e, string_view msg) {
            socket::send(e, "error: " + string(msg));
            cerr << "error: " << msg << endl;
        }
    }

    namespace socket_callback {
        void open(auto &&e) {
            cout << "New connection! " << socket::address(e) << endl;
        }

        template<typename T>
        void message(T &&e, string_view message) {
            istringstream iss(string {message});
            string command;
            iss >> command;

            using Command = function<void(istream &)>;

            static std::map<string, Command> commands {
                {"new_user", stream_cmd(server_request::new_user, e)},
                {"sign_in", stream_cmd(server_request::sign_in, e)},
                {"sign_out", stream_cmd(server_request::sign_out, e)},
            };

            try {
                commands.at(command)(iss);
            } catch (exception const &) {
                impl::systemError(e, "error: Cannot parse command - " + command);
            }
        }
    }

    namespace server {
        auto start(uWS::App &&s, unsigned port) {
            using UserData = decay_t<decltype(socket::get_user_data(impl::Socket {}))>;

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
