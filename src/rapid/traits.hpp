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

    namespace server {
        template<typename S>
        struct SignControl {
            using session_type = std::nullptr_t;

            static optional<session_type> signIn(S e, string_view name, string_view password);

            static void signOut(S e, session_type session);
        };

        template<typename S>
        struct FileAccessControl {
            using user_type = nullptr_t;

            using group_type = nullptr_t;

            using file_type = nullptr_t;

            static bool canReadFile(S e, user_type user, file_type file);

            static void addUserAccessFile(S e, user_type user, file_type file);

            static void addGroupAccessFile(S e, group_type group, file_type file);
        };

        template<typename S>
        struct UserControl {
            using user_type = nullptr_t;

            using group_type = nullptr_t;

            using session_type = nullptr_t;

            static optional<session_type> newUser(S e, string_view name, string_view password);

            static void addUserGroup(S e, user_type user, group_type group);
        };

        template<typename T>
        struct ServerResponse {
            static void newUser(T e, Success<string_view>);

            static void newUser(T e, Error<string_view>);

            static void signIn(T e, Success<string_view>);

            static void signIn(T e, Error<string_view>);

            static void signOut(T e);

            static void connectFarm(T e, Success<string_view>);

            static void connectFarm(T e, Error<string_view>);
        };

        template<typename T>
        struct ServerRequest {
            static void newUser(T e, string_view login, string_view password);

            static void signIn(T e, string_view login, string_view password);

            static void signOut(T e);

            static void connectFarm(T e, string_view farm, string_view farmPassword);
        };

        template<typename T, typename UserDataT>
        struct UserDataControl {
            static UserDataT const* getUserData(T e);

            static UserDataT* getMutUserData(T e);
        };
    }

    namespace farm {
        template<typename T>
        struct ServerResponse {
            static void connectFarm(T e, Success<string_view>);

            static void connectFarm(T e, Error<string_view>);
        };

        template<typename T>
        struct ServerRequest {
            void connectFarm(string_view farm, string_view farmPassword);
        };
    }
}