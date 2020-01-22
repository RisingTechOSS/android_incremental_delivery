/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "dataloader_ndk.h"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace android::dataloader {

using DataLoaderStatus = ::DataLoaderStatus;

struct DataLoader;
struct DataLoaderParams;
struct FilesystemConnector;
struct StatusListener;

using Inode = IncFsInode;
using PendingReadInfo = IncFsPendingReadInfo;
using PageReadInfo = IncFsPageReadInfo;

using FilesystemConnectorPtr = FilesystemConnector*;
using StatusListenerPtr = StatusListener*;
using ServiceConnectorPtr = DataLoaderServiceConnectorPtr;
using ServiceParamsPtr = DataLoaderServiceParamsPtr;

using DataLoaderPtr = std::unique_ptr<DataLoader>;
using PendingReads = std::span<const PendingReadInfo>;
using PageReads = std::span<const PageReadInfo>;
using RawMetadata = std::vector<char>;

constexpr int kBlockSize = INCFS_DATA_FILE_BLOCK_SIZE;

struct DataLoader {
    using Factory = std::function<DataLoaderPtr(DataLoaderServiceVmPtr)>;
    static void initialize(Factory&& factory);

    virtual ~DataLoader() {}

    // Lifecycle.
    virtual bool onCreate(const DataLoaderParams&, FilesystemConnectorPtr, StatusListenerPtr,
                          ServiceConnectorPtr, ServiceParamsPtr) = 0;
    virtual bool onStart() = 0;
    virtual void onStop() = 0;
    virtual void onDestroy() = 0;

    // FS callbacks.
    virtual bool onPrepareImage(jobject addedFiles, jobject removedFiles) { return false; }

    // IFS callbacks.
    virtual void onPendingReads(const PendingReads& pendingReads) = 0;
    virtual void onPageReads(const PageReads& pageReads) = 0;
};

struct DataLoaderParams {
    int type() const { return mType; }
    const std::string& packageName() const { return mPackageName; }
    const std::string& className() const { return mClassName; }
    const std::string& arguments() const { return mArguments; }

    struct NamedFd {
        std::string name;
        int fd;
    };
    const std::vector<NamedFd>& dynamicArgs() const { return mDynamicArgs; }

    DataLoaderParams(int type, std::string&& packageName, std::string&& className,
                     std::string&& arguments, std::vector<NamedFd>&& dynamicArgs);

private:
    int const mType;
    std::string const mPackageName;
    std::string const mClassName;
    std::string const mArguments;
    std::vector<NamedFd> const mDynamicArgs;
};

struct FilesystemConnector : public DataLoaderFilesystemConnector {
    int writeBlocks(const incfs_new_data_block blocks[], int blocksCount);
    RawMetadata getRawMetadata(Inode ino);
};

struct StatusListener : public DataLoaderStatusListener {
    bool reportStatus(DataLoaderStatus status);
};

} // namespace android::dataloader

#include "dataloader_inline.h"