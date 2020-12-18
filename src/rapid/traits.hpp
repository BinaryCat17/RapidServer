#pragma once
#include <string_view>
#include <optional>

namespace rapid {
    using namespace std;

    template<typename T>
    struct Success {
        T const &v;
    };

    template<typename T>
    struct Error {
        T const &v;
    };

    template<typename Trait, typename T = void, typename... Types>
    struct implements : implements<Trait, Types...> {};

    template<typename Trait, typename... Types>
    struct implements<Trait, Trait, Types...> { constexpr static bool value = true; };

    template<typename T>
    struct implements<T, void> { constexpr static bool value = false; };

    struct unused_t {};
    constexpr unused_t unused = unused_t{};

    template<typename... Types>
    constexpr bool implements_v = implements<Types...>::value;

    template<typename T, typename... Types>
    struct Merge : Merge<Types...> {
        explicit Merge(auto &&m, auto &&... args) : Merge<Types...>(args...), v(forward<T>(m)) {}

        operator T &() { return v; }

        operator T const &() const { return v; }

    private:
        T v;
    };

    template<typename T>
    struct Merge<T> {
        explicit Merge(auto &&m) : v(forward<T>(m)) {}

        operator T &() { return v; }

        operator T const &() const { return v; }

    private:
        T v;
    };

    template<typename... Types, typename... Types2>
    struct Merge<Merge<Types...>, Types2...> : Merge<Types...>, Merge<Types2...> {
        explicit Merge(auto &&m, auto &&... args) :
            Merge<Types2...>(args...), Merge(forward<Merge<Types...>>(m)) {}
    };

    template<typename... Types>
    Merge(Types...) -> Merge<Types...>;

   template<typename... Types, typename... Types2>
    Merge(Merge<Types...>, Types2...) -> Merge<Merge<Types...>, Types2...>;

    template<typename From, typename To>
    concept take_ref = convertible_to < From const&, To const&>;

    template<typename From, typename To>
    concept take_mut_ref = convertible_to<From &, To &>;

    namespace sign_control {
        struct Default {};

        template<typename T, typename... TT>
        optional<typename T::session_type>
        sign_in(T &&, Traits<TT...>, Traits<TT...>, string_view /*name*/, string_view /*pass*/) {
            static_assert((sizeof(T)) == 0, "Trait sign_control::sign_in is not implemented");
        }

        template<typename T, typename... TT>
        void sign_out(T &&, Traits<TT...>, typename T::session_type /*session*/) {
            static_assert((sizeof(T)) == 0, "Trait sign_control::sign_out is not implemented");
        }
    }

    namespace file_access {
        struct Default {};

        template<typename T, typename... TT>
        bool can_read_file(T &&, Traits<TT...>, typename T::user_type, typename T::file_type) {
            static_assert((sizeof(T)) == 0, "Trait file_access::can_read_file is not implemented");
            return {};
        }

        template<typename T, typename... TT>
        void
        add_user_access_file(T &&, Traits<TT...>, typename T::user_type, typename T::file_type) {
            static_assert((sizeof(T)) == 0,
                "Trait file_access::add_user_access_file is not implemented");
        }

        template<typename T, typename... TT>
        void
        add_group_access_file(T &&, Traits<TT...>, typename T::group_type, typename T::file_type) {
            static_assert((sizeof(T)) == 0,
                "Trait file_access::add_group_access_file is not implemented");
        }
    }

    namespace user_control {
        struct Default {};

        template<typename T, typename... TT>
        auto new_user(T &&, Traits<TT...>, string_view, string_view) {
            static_assert((sizeof(T)) == 0, "Trait user_control::new_user is not implemented");
            return 0;
        }

        template<typename T, typename... TT>
        void add_user_group(T &&, Traits<TT...>, auto && user, auto && group) {
            static_assert((sizeof(T)) == 0,
                "Trait user_control::add_user_group is not implemented");
        }
    }

    namespace socket {
        struct Default {};

        template<typename T, typename... TT>
        auto get_user_data(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait socket::get_user_data is not implemented");
            return 0;
        }

        template<typename T, typename... TT>
        auto get_mut_user_data(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait socket::get_mut_user_data is not implemented");
            return 0;
        }

        template<typename T, typename... TT>
        void send(T &&, Traits<TT...>, string_view /* message */) {
            static_assert((sizeof(T)) == 0, "Trait socket::send is not implemented");
        }

        template<typename T, typename... TT>
        string address(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait socket::address is not implemented");
            return {};
        }
    }

    namespace server_response {
        struct Default {};

        template<typename T, typename... TT>
        void new_user(T &&e, Traits<TT...> t, Success<string_view> session) {
            socket::send(e, t, "newUser success " + string(session.v));
        }

        template<typename T, typename... TT>
        void new_user(T &&e, Traits<TT...> t, Error<string_view> error) {
            socket::send(e, t, "newUser error " + string(error.v));
        }

        template<typename T, typename... TT>
        void sign_in(T &&e, Traits<TT...> s, Success<string_view> session) {
            socket::send(e, s, "signIn success " + string(session.v));
        }

        template<typename T, typename... TT>
        void sign_in(T &&e, Traits<TT...> s, Error<string_view> error) {
            socket::send(e, s, "signIn error " + string(error.v));
        }

        template<typename T, typename... TT>
        void sing_out(T &&e, Traits<TT...> t) {
            socket::send(e, t, "signOut success");
        }
    }

    namespace server_request {
        struct Default {};

        template<typename T, typename... TT>
        void new_user(T &&e, Traits<TT...> t, string_view login, string_view password) {
            if (auto user = user_control::new_user(e, t, login, password)) {
                *socket::get_mut_user_data(e, t) = *user;
            } else {
                server_response::new_user(e, t, Error<string_view> {"User already exist!"});
            }
        }

        template<typename T, typename... TT>
        void sign_in(T &&e, Traits<TT...> t, string_view login, string_view password) {
            auto data = socket::get_mut_user_data(e, t);
            if (!*data) {
                server_response::sign_in(e, Error<string_view> {"Already signed in!"});
                return;
            }
            if (auto user = sign_control::sign_in(e, t, login, password)) {
                *data = *user;
            } else {
                server_response::sign_in(e, t, Error<string_view> {"Incorrect login or password"});
            }
        }

        template<typename T, typename... TT>
        void sign_out(T &&e, Traits<TT...> t) {
            sign_control::sign_out(e, t, *socket::get_user_data(e));
        }
    }

    namespace response {
        struct Default {};

        template<typename T, typename... TT>
        void write(T &&, Traits<TT...>, string_view) {
            static_assert((sizeof(T)) == 0, "Trait response::write is not implemented");
        }
    }

    namespace request {
        struct Default {};

        template<typename T, typename... TT>
        string_view url(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait request::url is not implemented");
            return {};
        }
    }

    namespace server_callback {
        struct Default {};

        template<typename T, typename... TT>
        void get(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait server_callback::get is not implemented");
        }
    }

    namespace socket_callback {
        struct Default {};

        template<typename T, typename... TT>
        void open(T &&, Traits<TT...>) {
            static_assert((sizeof(T)) == 0, "Trait socket_callback::open is not implemented");
        }

        template<typename T, typename... TT>
        void message(T &&, Traits<TT...>, string_view) {
            static_assert((sizeof(T)) == 0, "Trait socket_callback::message is not implemented");
        }
    }

    namespace explorer {
        struct Default {};

        template<typename T, typename... TT>
        auto make(T &&, Traits<TT...>, filesystem::path const &/*root*/) {
            static_assert((sizeof(T)) == 0, "Trait explorer::make is not implemented");
            return;
        }

        template<typename T, typename... TT>
        string_view file(T &&, Traits<TT...>, fs::path const &) {
            static_assert((sizeof(T)) == 0, "Trait explorer::file is not implemented");
            return {};
        }
    }

    namespace server {
        struct Default { constexpr static bool is_trait = true; };

        template<typename T, typename... TT>
        auto start(T &&, Traits<TT...>, unsigned /*port*/) {
            static_assert((sizeof(T)) == 0, "Trait server::start is not implemented");
            return;
        }
    }

    namespace storage {
        struct Default {};

        template<typename T, typename... TT>
        auto make(T &&, Traits<TT...>, filesystem::path const &, string const &) {
            static_assert((sizeof(T)) == 0, "Trait storage::make is not implemented");
            return;
        }
    }
}