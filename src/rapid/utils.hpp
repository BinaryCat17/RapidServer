#pragma once
#include <functional>
#include <iostream>
#include <concepts>
#include <vector>
#include <filesystem>
#include <string>

namespace rapid {
    using namespace std;
    namespace fs = std::filesystem;

    template<typename FnT, typename... Types>
    auto curry(FnT &&f, Types &&... values) {
        return [&](auto &&... args) {
            return f(values..., args...);
        };
    }

    template<typename FnT>
    auto curry(FnT &&f) {
        return [&](auto &&... args) {
            return curry(f, args...);
        };
    }

    template<typename T, typename R>
    auto doStream(function<R(T)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        return f(val);
    }

    template<typename T, typename... Types, typename R>
    auto doStream(function<R(T, Types...)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        auto f2 = curry(f);
        return doStream(function<R(Types...)>(f2(val)), is);
    }

    template<typename... Types>
    auto stream(function<void(Types...)> f) {
        return [f](istream&is) {
            return doStream(f, is);
        };
    }

    inline auto stream(function<void()> f) {
        return [f](istream&) {
            return f();
        };
    }

    template<typename... Types, typename FnT>
    auto cmd(FnT && f) {
        return stream(function<void(Types...)>(f));
    }

    template<typename T = void, typename... Types>
    struct Traits {};

    template<>
    struct Traits<void> {};

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
