#pragma once
#include <functional>
#include <iostream>
#include <concepts>
#include <vector>

namespace rapid {
    using namespace std;

    template<typename FnT, typename... Types>
    auto curry(FnT &&f, Types &&... values) {
        return [=](auto &&... args) {
            return f(values..., args...);
        };
    }

    template<typename FnT>
    auto curry(FnT &&f) {
        return [=](auto &&... args) {
            return curry(f, args...);
        };
    }

    template<typename R, typename T>
    auto stream(function<R(T)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        return f(val);
    }

    template<typename R, typename T, typename... Types>
    auto stream(function<R(T, Types...)> f, istream &is) {
        decay_t<T> val;
        is >> val;
        auto f2 = carry(f);
        return doStream(function<R(Types...)>(f2(val)), is);
    }

    template<typename FnT>
    auto stream(FnT && f, string_view s) {
        return stream(f, istringstream(f));
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
}
