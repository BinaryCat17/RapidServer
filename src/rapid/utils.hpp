#pragma once
#include <functional>
#include <iostream>
#include <concepts>
#include <vector>
#include <filesystem>
#include <string>
#include "./curry.hpp"

namespace rapid {
    using namespace std;
    namespace fs = std::filesystem;

//    template<typename FnT, typename... Types>
//    auto curry(FnT &&f, Types &&... values) {
//        return [&](auto &&... args) {
//            return f(values..., args...);
//        };
//    }
//
//    template<typename FnT>
//    auto curry(FnT &&f) {
//        return [&](auto &&... args) {
//            return curry(f, args...);
//        };
//    }

    template<typename R, typename T>
    auto doStream(function<R(T)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        return f(val);
    }

    template<typename R, typename T, typename... Types>
    auto doStream(function<R(T, Types...)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        auto f2 = curry(f);
        return doStream(function<R(Types...)>(f2(val)), is);
    }

    template<typename FnT>
    auto stream(FnT &&f) {
        return curry<FnT>(doStream)(forward<FnT>(f));
    }

    template<typename T = void, typename... Types>
    struct Traits {};

    template<>
    struct Traits<void> {};

    template<typename FnT, typename T, typename... TT>
    auto stream_cmd(FnT && f, T && v, Traits<TT...> traits) {
        return stream(curry(f, v, traits));
    }

    template<typename Cont, typename VT, typename FnT>
    auto const &atOrInsert(Cont &c, VT const &v, FnT &&f) {
        if (auto iter = c.find(v); iter != c.end()) {
            return iter->second;
        } else {
            auto item = c.emplace(v, f());
            return item.first->second;
        }
    }

    template<template<typename> class ConT, typename T, typename FnT, typename R = invoke_result_t<FnT, T>>
    requires (!is_same_v<R, void>)
    auto map(ConT<T> const &cont, FnT &&) {
        ConT<R> result;
        for (auto &v : cont) {
            result.push_back(v);
        }
        return result;
    }

    template<template<typename> class ConT, typename T, typename FnT, typename R = invoke_result_t<FnT, T>>
    requires is_same_v<R, void>
    auto map(ConT<T> const &cont, FnT &&f) {
        for (auto &v : cont) {
            f(v);
        }
    }

    template<template<typename> class ConT, typename T, typename FnT, typename R = invoke_result_t<FnT, T>>
    requires (!is_same_v<R, void>)
    auto map(ConT<T> &cont, FnT &&) {
        ConT<R> result;
        for (auto &v : cont) {
            result.push_back(v);
        }
        return result;
    }

    template<template<typename> class ConT, typename T, typename FnT, typename R = invoke_result_t<FnT, T>>
    requires is_same_v<R, void>
    auto map(ConT<T> &cont, FnT &&f) {
        for (auto &v : cont) {
            f(v);
        }
    }

    template<typename FnT>
    auto catchf(FnT &&f) {
        try {
            return f();
        } catch (exception const &e) {
            cerr << e.what();
        }
    }



    string readFile(fs::path const &f);
}
