#pragma once

#include <utility>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>

namespace sqlgui::core {

template <typename T, typename E>
using Expected = std::expected<T, E>;

template <typename E>
using Unexpected = std::unexpected<E>;

template <typename E>
auto make_unexpected(E&& error) {
    return std::unexpected<std::decay_t<E>>(std::forward<E>(error));
}

}  // namespace sqlgui::core

#else

#include <tl/expected.hpp>

namespace sqlgui::core {

template <typename T, typename E>
using Expected = tl::expected<T, E>;

template <typename E>
using Unexpected = tl::unexpected<E>;

template <typename E>
auto make_unexpected(E&& error) {
    return tl::make_unexpected(std::forward<E>(error));
}

}  // namespace sqlgui::core

#endif
