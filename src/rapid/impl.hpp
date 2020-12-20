#include "utils.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include "sqliteUtils.hpp"
#include <map>
#include <concepts>
#include <uWebSockets/App.h>

namespace rapid {
    template<typename T>
    struct Success {
        T const &v;
    };

    template<typename T>
    struct Error {
        T const &v;
    };

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

        struct Farm {
            int id;
            int userId;
            int farmId;
        };

        struct CacheExplorer {
            fs::path mRoot;
            std::map<fs::path, string> mCache;
        };

        using Socket = uWS::WebSocket<false, true> *;

        using Response = uWS::HttpResponse<false> *;
        using Request = uWS::HttpRequest *;
        struct UserData {
            optional<int> user;
            optional<int> session;
            optional<int> farmSession;
        };
    }

    namespace storage {
        auto make(filesystem::path const &root, string_view filename) {
            using namespace db;
            using namespace impl;

            fs::create_directory(root);

            auto s = make_storage(root / string(filename),
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
                    make_column("FileId", &FileAccessUser::fileId),
                    make_column("UserId", &FileAccessUser::userId)),
                make_table("Session",
                    make_column("Id", &Session::id, autoincrement(), primary_key()),
                    make_column("UserId", &Session::userId)),
                make_table("Farm",
                    make_column("Id", &Farm::id, autoincrement(), primary_key()),
                    make_column("UserId", &Farm::userId),
                    make_column("FarmId", &Farm::farmId))
            );

            return s;
        }
    }

    namespace impl {
        using Storage = decay_t<decltype(storage::make("", ""))> *;
    }

    namespace sign_control {
        optional<int> sign_in(impl::Storage s, string const &name, string const &pass) {
            if (auto user = getByName<impl::User>(s, name)) {
                if (user->password == pass) {
                    impl::Session session {.id = -1, .userId = user->id};
                    return insertTo(s, session);
                }
            }
            return {};
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
        optional<int> new_user(impl::Storage s, string const &name, string const &pass) {
            using namespace impl;
            using namespace db;

            if (getByName<User>(s, name)) {
                return {};
            }

            return {s->insert(User {-1, name, pass})};
        }

        bool add_user_group(impl::Storage s, int user, int group) {
            using namespace db;
            using namespace impl;
            if (s->get_all<UserGroup>(where(
                bitwise_and(is_equal(&UserGroup::userId, user),
                    is_equal(&UserGroup::groupId, group)))).empty()) {
                s->insert(impl::UserGroup {.userId = user, .groupId = group});
                return true;
            } else {
                return false;
            }
        }
    }

    namespace user_data {
        impl::UserData const *get_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        impl::UserData *get_mut_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }
    }

    namespace explorer {
        auto make(filesystem::path const &root) {
            return impl::CacheExplorer {.mRoot = root};
        }

        string_view file(impl::CacheExplorer &s, fs::path const &f) {
            return atOrInsert(s.mCache, f, [&] { return readFile(s.mRoot / f); });
        }
    }

    namespace socket {
        auto const *get_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData const *>(s->getUserData());
        }

        auto *get_mut_user_data(impl::Socket s) {
            return reinterpret_cast<impl::UserData *>(s->getUserData());
        }

        bool is_sign_in(impl::Socket s) {
            return get_user_data(s)->user.has_value();
        }

        void send(impl::Socket s, string_view message) {
            cout << "Sending message: " << message << endl;
            s->send(message, uWS::OpCode::TEXT);
        }

        string address(impl::Socket s) {
            return string(s->getRemoteAddressAsText());
        }
    }

    namespace server_response {
        void new_user(impl::Socket socket, Success<string_view> session) {
            socket::send(socket, "new_user success " + string(session.v));
        }

        void new_user(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "new_user error " + string(error.v));
        }

        void sign_in(impl::Socket socket, Success<string_view> session) {
            socket::send(socket, "sign_in success " + string(session.v));
        }

        void sign_in(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "sign_in error " + string(error.v));
        }

        void sign_out(impl::Socket socket) {
            socket::send(socket, "sign_out success");
        }

        void sign_out(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "sign_out error " + string(error.v));
        }

        void connect_farm(impl::Socket socket, Success<string_view> session) {
            socket::send(socket, "connect_farm success " + string(session.v));
        }

        void connect_farm(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "connect_farm error " + string(error.v));
        }

        void new_farm(impl::Socket socket, Success<string_view> session) {
            socket::send(socket, "new_farm success " + string(session.v));
        }

        void new_farm(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "new_farm error " + string(error.v));
        }

        void disconnect_farm(impl::Socket socket) {
            socket::send(socket, "disconnect_farm success");
        }

        void disconnect_farm(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "disconnect_farm error " + string(error.v));
        }

        void set_temperature(impl::Socket socket) {
            socket::send(socket, "set_temperature success");
        }

        void set_temperature(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "set_temperature error " + string(error.v));
        }

        void set_humidity(impl::Socket socket) {
            socket::send(socket, "set_humidity success");
        }

        void set_humidity(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "set_humidity error " + string(error.v));
        }

        void set_light_interval(impl::Socket socket) {
            socket::send(socket, "set_temperature success");
        }

        void set_light_interval(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "set_light_interval error " + string(error.v));
        }

        void set_pump_interval(impl::Socket socket) {
            socket::send(socket, "set_pump_interval success");
        }

        void set_pump_interval(impl::Socket socket, Error<string_view> error) {
            socket::send(socket, "set_pump_interval error " + string(error.v));
        }
    }

    namespace impl {
        optional<impl::Farm> get_farm(auto &&f, impl::Storage storage, impl::Socket socket) {
            auto data = socket::get_mut_user_data(socket);

            if (!socket::is_sign_in(socket)) {
                f(socket, Error<string_view> {"Not sign in!"});
                return {};
            }

            using namespace db;
            auto farm = storage->get_all<impl::Farm>(
                where(is_equal(&impl::Farm::userId, *data->user)));

            if (farm.empty()) {
                f(socket, Error<string_view> {"Farm not found!"});
                return {};
            }

            return {farm.front()};
        }

//        optional<int> get_farm_session(auto &&f, impl::Storage storage, impl::Socket socket) {
//            using namespace db;
//            if (auto farm = get_farm(f, storage, socket)) {
//                auto session = storage->get_all<impl::Session>(
//                    where(is_equal(&impl::Session::userId, farm->farmId)));
//                if (session.empty()) {
//                    f(socket, Error<string_view> {"Farm not connected!"});
//                    return {};
//                }
//                return { session.front().id };
//            } else {
//                return {};
//            }
//        }
//
//        bool check_farm(auto &&f, impl::Storage storage, impl::Socket socket) {
//            using namespace db;
//            if (auto farm = get_farm(f, storage, socket)) {
//                auto session = storage->get_all<impl::Session>(
//                    where(is_equal(&impl::Session::userId, farm->farmId)));
//
//                if (!session.empty()) {
//                    f(socket, Error<string_view> {"Farm already connected!"});
//                    return false;
//                }
//
//                return true;
//            }
//            return false;
//        }

        void
        send_message_to_farm(auto &&f, uWS::App &app, impl::Socket socket, string_view message) {
            auto data = socket::get_user_data(socket);
            if (socket::is_sign_in(socket)) {
                if (data->farmSession) {
                    app.publish("arduino_" + to_string(*data->farmSession), message,
                        uWS::OpCode::TEXT);
                    f(socket);
                } else {
                    f(socket, Error<string_view> {"Farm not connected!"});
                }
            } else {
                f(socket, Error<string_view> {"Not signed in yet!"});
            }
        }

        bool check_farm(auto &&f, impl::Storage storage, impl::Socket socket) {
            using namespace db;
            if (get_farm(f, storage, socket)) {
                if (socket::get_user_data(socket)->farmSession) {
                    cout << "Farm session: " << *socket::get_user_data(socket)->farmSession << endl;
                    f(socket, Error<string_view> {"Farm already connected!"});
                    return false;
                }
                return true;
            }
            return false;
        }

        int get_user(impl::Storage storage, int session) {
            using namespace db;
            return storage->get<impl::Session>(session).userId;
        }

        bool is_farm(auto &&f, impl::Storage storage, impl::Socket socket, int id) {
            using namespace db;
            int group = getByName<impl::Group>(storage, "farm")->id;

            if (storage->get_all<impl::UserGroup>(where(
                is_equal(&UserGroup::userId, id) &&
                    is_equal(&UserGroup::groupId, group))).empty()) {
                f(socket, Error<string_view> {"It is not farm!"});
                return false;
            }
            return true;
        }
    }

    namespace server_request {
        void connect_farm(impl::Storage storage, impl::Socket socket, string const &id,
            string const &pass) {
            auto data = socket::get_mut_user_data(socket);

            using namespace impl;
            using namespace db;

            auto call = [](auto &&... a) { server_response::connect_farm(a...); };
            if (!check_farm(call, storage, socket)) return;

            if (auto session = sign_control::sign_in(storage, "farm_" + id, pass)) {
                if (!is_farm(call, storage, socket, storage->get<Session>(*session).userId)) {
                    sign_control::sign_out(storage, *session);
                    return;
                }

                data->farmSession.emplace(*session);
                server_response::connect_farm(socket, Success<string_view> {to_string(*session)});
            } else {
                server_response::connect_farm(socket,
                    Error<string_view> {"Incorrect farm login or password"});
            }
        }

        void
        new_farm(impl::Storage storage, impl::Socket socket, string const &id, string const &pass) {
            auto data = socket::get_mut_user_data(socket);

            if (!socket::is_sign_in(socket)) {
                server_response::new_farm(socket, Error<string_view> {"Not sign in!"});
                return;
            }

            if (auto farm = user_control::new_user(storage, "farm_" + id, pass)) {
                storage->insert(impl::Farm {.userId = *data->user, .farmId = *farm});
                int group = getByName<impl::Group>(storage, "farm")->id;
                if (!user_control::add_user_group(storage, *farm, group)) {
                    server_response::new_farm(socket, Error<string_view> {"Farm already exist!!"});
                }
                connect_farm(storage, socket, id, pass);
            } else {
                server_response::new_farm(socket, Error<string_view> {"Farm already exist!"});
            }
        }

        void disconnect_farm(impl::Storage storage, impl::Socket socket) {
            auto session = socket::get_mut_user_data(socket)->farmSession;
            if (!session) {
                server_response::disconnect_farm(socket,
                    Error<string_view> {"Farm not connected!"});
                return;
            }

            if (auto farm = impl::get_farm([](auto &&... a) {
                server_response::disconnect_farm(a...);
            }, storage, socket)) {
                sign_control::sign_out(storage, farm->farmId);
                storage->remove<impl::Session>(*session);
                socket::get_mut_user_data(socket)->farmSession.reset();
                server_response::disconnect_farm(socket);
            }
        }

        void set_temperature(uWS::App &app, impl::Socket socket, float temperature) {
            auto call = [](auto &&... a) { server_response::set_temperature(a...); };
            impl::send_message_to_farm(call, app, socket,
                "set_temperature " + to_string(temperature));
        }

        void set_humidity(uWS::App &app, impl::Socket socket, int v) {
            auto call = [](auto &&... a) { server_response::set_humidity(a...); };
            impl::send_message_to_farm(call, app, socket, "set_humidity " + to_string(v));
        }

        void set_light_interval(uWS::App &app, impl::Socket socket, int start, int end) {
            auto call = [](auto &&... a) { server_response::set_light_interval(a...); };
            impl::send_message_to_farm(call, app, socket,
                "set_light_interval " + to_string(start) + " " + to_string(end));

        }

        void set_pump_interval(uWS::App &app, impl::Socket socket, int start, int end) {
            auto call = [](auto &&... a) { server_response::set_pump_interval(a...); };
            impl::send_message_to_farm(call, app, socket,
                "set_pump_interval " + to_string(start) + " " + to_string(end));
        }

        void sign_in(impl::Storage storage, impl::Socket socket, string const &login,
            string const &password) {
            auto data = socket::get_mut_user_data(socket);
            if (data->user) {
                server_response::sign_in(socket, Error<string_view> {"Already signed in!"});
                return;
            }
            if (auto session = sign_control::sign_in(storage, login, password)) {
                server_response::sign_in(socket, Success<string_view> {to_string(*session)});
                data->user.emplace(impl::get_user(storage, *session));
                data->session.emplace(*session);
                if (impl::is_farm([](auto &&...) {}, storage, socket, *data->user)) {
                    socket->subscribe("arduino_" + to_string(*session));
                } else {
                    socket->subscribe("client_" + to_string(*session));
                }
            } else {
                server_response::sign_in(socket,
                    Error<string_view> {"Incorrect login or password"});
            }
        }

        void new_user(impl::Storage storage, impl::Socket socket, string const &login,
            string const &password) {
            if (socket::is_sign_in(socket)) {
                server_response::new_user(socket, Error<string_view> {"Already signed in!"});
                return;
            }

            if (user_control::new_user(storage, login, password)) {
                sign_in(storage, socket, login, password);
            } else {
                server_response::new_user(socket, Error<string_view> {"User already exist!"});
            }
        }

        void sign_out(impl::Storage storage, impl::Socket socket) {
            if (socket::is_sign_in(socket)) {
                auto userData = socket::get_mut_user_data(socket);
                if (userData->farmSession) {
                    disconnect_farm(storage, socket);
                }
                sign_control::sign_out(storage, *userData->session);
                userData->user.reset();
                userData->session.reset();
                server_response::sign_out(socket);
            } else {
                server_response::sign_out(socket, Error<string_view> {"Not signed in yet!"});
            }
        }
    }

    namespace response {
        void write(impl::Response s, string_view message) {
            s->write(message);
            s->end();
        }
    }

    namespace request {
        string_view url(impl::Request s) {
            return s->getUrl();
        }
    }

    namespace server_callback {
        void get(impl::CacheExplorer explorer, impl::Response res, impl::Request req) {
            try {
                response::write(res, explorer::file(explorer, req->getUrl().substr(1)));
            } catch (exception const &err) {
                cerr << err.what() << endl;
                response::write(res, err.what());
            }
        }

        void getHome(impl::CacheExplorer explorer, impl::Response res, impl::Request) {
            try {
                response::write(res, explorer::file(explorer, "RapidControl.html"));
            } catch (exception const &err) {
                cerr << err.what() << endl;
                response::write(res, err.what());
            }
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
        void open(impl::Socket e) {
            cout << "New connection! " << socket::address(e) << endl;
        }

        void message(uWS::App &app, impl::Storage storage, impl::Socket e, string_view message) {
            istringstream iss(string {message});
            string command;
            iss >> command;

            using Command = function<void(istream &)>;

            static std::map<string, Command> commands {
                {"new_user", cmd<string, string>(curry(server_request::new_user, storage, e))},
                {"sign_in", cmd<string, string>(curry(server_request::sign_in, storage, e))},
                {"sign_out", cmd(curry(server_request::sign_out, storage, e))},
                {"new_farm", cmd<string, string>(curry(server_request::new_farm, storage, e))},
                {"connect_farm",
                    cmd<string, string>(curry(server_request::connect_farm, storage, e))},
                {"disconnect_farm", cmd(curry(server_request::disconnect_farm, storage, e))},
                {"set_temperature", cmd<float>(curry(server_request::set_temperature, app, e))},
                {"set_humidity", cmd<int>(curry(server_request::set_humidity, app, e))},
                {"set_light_interval",
                    cmd<int, int>(curry(server_request::set_light_interval, app, e))},
                {"set_pump_interval",
                    cmd<int, int>(curry(server_request::set_pump_interval, app, e))},
            };

            using namespace impl;
            using namespace db;

            try {
                auto user = socket::get_user_data(e)->user;
                if (user && impl::is_farm([](auto &&...) {}, storage, e, *user)) {
                    cout << "Command from farm: " << message << endl;
                    auto farm = storage->get_all<Farm>(where(is_equal(&Farm::farmId, *user)));
                    auto client = farm.front();
                    app.publish("client_" + to_string(client.userId), message, uWS::OpCode::TEXT);
                } else {
                    cout << "Command from client: " << endl;
                    commands.at(command)(iss);
                }

            } catch (exception const &err) {
                cerr << "Error " << err.what() << endl;
                impl::systemError(e, "error: Cannot parse command - " + command);
            }
        }

        void close(impl::Storage storage, impl::Socket e) {
            if (socket::is_sign_in(e)) {
                server_request::sign_out(storage, e);
            }
        }
    }

    namespace server {
        auto start(int port) {
            using UserData = decay_t<decltype(socket::get_user_data(impl::Socket {}))>;
            auto userExplorer = explorer::make("data/usr");
            auto publicExplorer = explorer::make("data/public");
            auto storage = storage::make("data/local", "database.db");

            uWS::App *pApp = nullptr;

            auto app = uWS::App().get("/*", [&](impl::Response res, impl::Request req) {
                server_callback::get(publicExplorer, res, req);
            }).get("/main", [&](impl::Response res, impl::Request req) {
                server_callback::getHome(publicExplorer, res, req);
            }).template ws<UserData>("/*", uWS::App::WebSocketBehavior {
                .open = [&](impl::Socket ws) {
                    socket_callback::open(ws);
                },
                .message = [&](impl::Socket ws, string_view message, uWS::OpCode) {
                    socket_callback::message(*pApp, &storage, ws, message);
                },
                .close = [&](impl::Socket ws, int, string_view) {
                    socket_callback::close(&storage, ws);
                }
            }).listen("0.0.0.0", port, [](auto *listenSocket) {
                if (listenSocket) {

                    cout << "Listening for connections..." << endl;
                }
            });

            pApp = &app;
            uWS::run();
        }
    }
}
