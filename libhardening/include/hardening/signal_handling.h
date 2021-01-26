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

#include <sys/types.h>

#if !defined(__BIONIC__)

// IncFS signal handling isn't needed anywhere but on Android as of now
#define HANDLE_SIGBUS(code)

#else

#ifndef LOG_TAG
#define LOG_TAG "hardening"
#endif

#include <log/log.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

namespace hardening {

struct JmpBufState final {
    jmp_buf buf;
    bool armed = false;

    JmpBufState() = default;
    JmpBufState(const JmpBufState& other) {
        if (other.armed) {
            memcpy(&buf, &other.buf, sizeof(buf));
            armed = true;
        }
    }

    JmpBufState& operator=(const JmpBufState& other) {
        if (other.armed) {
            memcpy(&buf, &other.buf, sizeof(buf));
            armed = true;
        } else {
            armed = false;
        }
        return *this;
    }
};

class ScopedBuf final {
public:
    ScopedBuf(const JmpBufState& prev) : mPrev(prev) {}
    ~ScopedBuf();

    ScopedBuf(const ScopedBuf&) = delete;

private:
    const JmpBufState& mPrev;
};

#define HANDLE_SIGBUS(code)                                                        \
    hardening::SignalHandler::instance();                                          \
    auto& tlsBuf = hardening::SignalHandler::mJmpBuf;                              \
    hardening::JmpBufState oldBuf_macro = tlsBuf;                                  \
    if (setjmp(tlsBuf.buf) != 0) {                                                 \
        ALOGI("%s: handling SIGBUS at line %d", __func__, __LINE__);               \
        tlsBuf = oldBuf_macro;                                                     \
        { code; }                                                                  \
        LOG_ALWAYS_FATAL("%s(): signal handler was supposed to return", __func__); \
    }                                                                              \
    tlsBuf.armed = true;                                                           \
    hardening::ScopedBuf oldBufRestore_macro(oldBuf_macro);

class SignalHandler final {
public:
    static SignalHandler& instance();

private:
    SignalHandler();
    inline static struct sigaction mOldSigaction = {};

    static void handler(int sig, siginfo_t* info, void* ucontext);

public:
    inline static thread_local JmpBufState mJmpBuf = {};
};

} // namespace hardening

#endif
