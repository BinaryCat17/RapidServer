#pragma once
#include "App.h"
#include "utils.hpp"

namespace rapid {
    using namespace std;

//    template<typename UserData, typename FnT> requires invocable<FnT, UserData *, string_view> &&
//        is_same_v<invoke_result_t < FnT, UserData *, string_view>, string>
//
//    auto response(FnT &&f) {
//        return [&]<bool SSL, bool isServer>(uWS::WebSocket<SSL, isServer> *ws,
//            std::string_view message, uWS::OpCode opCode) {
//            auto user = reinterpret_cast<UserData *>(ws->getUserData());
//            ws->send(f(user, message), opCode, true);
//        };
//    }
//
//    template<typename UserData, typename FnT, typename... Types> requires
//    invocable<FnT, UserData *, Types...> &&
//        is_same_v<invoke_result_t < FnT, UserData *, Types...>, string>
//
//    auto response(FnT &&f) {
//        return [&]<bool SSL, bool isServer>(uWS::WebSocket<SSL, isServer> *ws,
//            std::string_view message, uWS::OpCode opCode) {
//            auto user = reinterpret_cast<UserData *>(ws->getUserData());
//            auto cf = stream(curry(f, user), message);
//            ws->send(move(cf), opCode, true);
//        };
//    }
}