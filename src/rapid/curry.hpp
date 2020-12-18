#pragma once
#include "./fwd_capture.hpp"
//#include <./type_traits.hpp>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <experimental/tuple>
#include <functional>
#include <type_traits>

using vr::impl::apply_fwd_capture;

namespace impl
{
    template <typename T, typename TSource>
    using copy_referenceness_impl =
    std::conditional_t<!std::is_reference<TSource>{}, T,
        std::conditional_t<std::is_lvalue_reference<TSource>{},
            std::add_lvalue_reference_t<T>,
            std::add_rvalue_reference_t<T>>>;
}

/// @brief Removes reference from `T`, then applies `TSource`'s
/// "referenceness" to it.
template <typename T, typename TSource>
using copy_referenceness =
impl::copy_referenceness_impl<std::remove_reference_t<T>, TSource>;

template <typename T, typename TSource>
using as_if_forwarded = std::conditional_t<!std::is_reference<TSource>{},
    std::add_rvalue_reference_t<std::remove_reference_t<T>>,
    copy_referenceness<T, TSource>>;

/// @brief Casts the passed forwarding reference `x` as if it was being
/// forwarded as `TLike`.
template <typename TLike, typename T>
inline constexpr decltype(auto) forward_like(
    T && x) noexcept
{
    VRM_CORE_STATIC_ASSERT_NM(!(std::is_rvalue_reference<decltype(x)>{} &&
        std::is_lvalue_reference<TLike>{}));

    return static_cast<as_if_forwarded<T, TLike>>(x);
}

// clang-format off
template <typename TF>
constexpr decltype(auto) curry(TF&& f)
{
    // If `f()` can be called, then immediately call and return.
    // (Base case.)

    // Otherwise, return a function that allows partial application of any
    // number of arguments.
    // (Recursive case.)

    if constexpr (std::is_invocable<TF&&()>{})
    {
        // Base case.
        return FWD(f)();
    }
    else
    {
        // Recursive case.

        // Return a lambda that binds any number of arguments to the current
        // callable object `f` - this is "partial application".
        return [xf = FWD_CAPTURE(f)](auto&&... partials) mutable constexpr
        {//                                              ^^^^^^^
            // The `mutable` is very important as we'll be moving `f` to the
            // inner lambda.

            // As we may want to partial-apply multiple times (currying in the
            // case of a single argument), we need to recurse here.
            return curry
                (
                    [
                        partial_pack = FWD_CAPTURE_PACK_AS_TUPLE(partials),

                        // `xf` can be moved as it's a "forward-capture" wrapper.
                        yf = std::move(xf)
                    ]
                        (auto&&... xs) constexpr
                        // Weirdly `g++` doesn't like `decltype(auto)` here.
                        -> decltype(forward_like<TF>(xf.get())(FWD(partials)...,
                        FWD(xs)...))
                    {
                        // `yf` will be called by applying the concatenation of
                        // `partial_pack` and `xs...`, retaining the original value
                        // categories thanks to the "forward-capture" wrappers.
                        return apply_fwd_capture(
                            [
                                // `yf` can be captured by reference as it's just a
                                // wrapper which lives in the parent lambda.
                                &yf
                            ](auto&&... ys) constexpr
                                -> decltype(forward_like<TF>(yf.get())(FWD(ys)...))
                            {//                                        ^^^^^^^^^^
                                // The `ys...` pack will contain all the concatenated
                                // values.
                                //                                vvvvvvvvvv
                                return forward_like<TF>(yf.get())(FWD(ys)...);
                                //                      ^^^^^^^^
                                // `yf.get()` is either the original callable object or
                                // an intermediate step of the `curry` recursion.
                            }, partial_pack, FWD_CAPTURE_PACK_AS_TUPLE(xs));
                        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                        // Automatically concatenated by `apply_fwd_capture`.
                    }
                );
        };
    }
}
