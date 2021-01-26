/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <hardening/signal_handling.h>

#include <optional>
#include <type_traits>

namespace hardening {

template <class Arg, class Func>
using func_result = std::remove_cvref_t<decltype(std::declval<Func>()(std::declval<Arg>()))>;

template <class Arg, class Func>
constexpr auto is_void_func = std::is_same_v<func_result<Arg, Func>, void>;

template <class Arg, class Func>
using optional_result =
        std::conditional_t<is_void_func<Arg, Func>, bool, std::optional<func_result<Arg, Func>>>;

template <class Ptr, class F>
auto access(Ptr ptr, F&& accessor) -> optional_result<Ptr, F> {
    HANDLE_SIGBUS({ return {}; });

    if constexpr (is_void_func<Ptr, F>) {
        accessor(ptr);
        return true;
    } else {
        return accessor(ptr);
    }
}

} // namespace hardening