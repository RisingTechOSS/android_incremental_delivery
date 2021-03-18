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

#include "MountRegistry.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <sys/select.h>
#include <unistd.h>

#include <iterator>
#include <optional>
#include <thread>

#include "incfs.h"
#include "path.h"

using namespace android::incfs;
using namespace std::literals;

class MountRegistryTest : public ::testing::Test {
protected:
    virtual void SetUp() {}
    virtual void TearDown() {}

    MountRegistry::Mounts mounts_;

    MountRegistry::Mounts& r() { return mounts_; }
};

TEST_F(MountRegistryTest, RootForRoot) {
    r().addRoot("/root", "/backing");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1/2").data());
    ASSERT_STREQ(nullptr, r().rootFor("/root1/1/2").data());
    ASSERT_STREQ(nullptr, r().rootFor("/1/root").data());
    ASSERT_STREQ(nullptr, r().rootFor("root").data());
}

TEST_F(MountRegistryTest, OneBind) {
    r().addRoot("/root", "/backing");
    r().addBind("/root/1", "/bind");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/bind").data());
    ASSERT_STREQ("/root", r().rootFor("/bind/1").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1").data());
    ASSERT_STREQ(nullptr, r().rootFor("/1/bind").data());
    ASSERT_STREQ(nullptr, r().rootFor("bind").data());
    ASSERT_STREQ(nullptr, r().rootFor("/bind1").data());
    ASSERT_STREQ(nullptr, r().rootFor("/.bind").data());
}

TEST_F(MountRegistryTest, MultiBind) {
    r().addRoot("/root", "/backing");
    r().addBind("/root/1", "/bind");
    r().addBind("/root/2/3", "/bind2");
    r().addBind("/root/2/3", "/other/bind");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/bind").data());
    ASSERT_STREQ("/root", r().rootFor("/bind2").data());
    ASSERT_STREQ("/root", r().rootFor("/other/bind/dir").data());
    ASSERT_EQ("/root"s, r().rootAndSubpathFor("/root").first->path);
    ASSERT_EQ(""s, r().rootAndSubpathFor("/root").second);
    ASSERT_EQ("/root"s, r().rootAndSubpathFor("/bind").first->path);
    ASSERT_EQ("1"s, r().rootAndSubpathFor("/bind").second);
    ASSERT_EQ("/root"s, r().rootAndSubpathFor("/bind2").first->path);
    ASSERT_EQ("2/3"s, r().rootAndSubpathFor("/bind2").second);
    ASSERT_EQ("/root"s, r().rootAndSubpathFor("/bind2/blah").first->path);
    ASSERT_EQ("2/3/blah"s, r().rootAndSubpathFor("/bind2/blah").second);
    ASSERT_EQ("/root"s, r().rootAndSubpathFor("/other/bind/blah").first->path);
    ASSERT_EQ("2/3/blah"s, r().rootAndSubpathFor("/other/bind/blah").second);
}

TEST_F(MountRegistryTest, MultiRoot) {
    r().addRoot("/root", "/backing");
    r().addBind("/root", "/bind");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/bind").data());
    ASSERT_STREQ("/root", r().rootFor("/bind/2").data());
}

TEST_F(MountRegistryTest, MultiRootLoad) {
    constexpr char mountsFile[] =
            R"(4605 34 0:154 / /mnt/installer/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime shared:45 master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4561 35 0:154 / /mnt/androidwritable/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime shared:44 master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4560 99 0:154 / /storage/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4650 30 0:44 /MyFiles /mnt/pass_through/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,relatime shared:31 - 9p media rw,sync,dirsync,access=client,trans=virtio
3181 79 0:146 / /data/incremental/MT_data_app_vmdl703/mount rw,nosuid,nodev,noatime shared:46 - incremental-fs /data/incremental/MT_data_app_vmdl703/backing_store rw,seclabel,read_timeout_ms=10000,readahead=0
3182 77 0:146 / /var/run/mount/data/mount/data/incremental/MT_data_app_vmdl703/mount rw,nosuid,nodev,noatime shared:46 - incremental-fs /data/incremental/MT_data_app_vmdl703/backing_store rw,seclabel,read_timeout_ms=10000,readahead=0
)";

    TemporaryFile f;
    ASSERT_TRUE(android::base::WriteFully(f.fd, mountsFile, std::size(mountsFile) - 1));
    ASSERT_EQ(0, lseek(f.fd, 0, SEEK_SET)); // rewind

    MountRegistry::Mounts m;
    ASSERT_TRUE(m.loadFrom(f.fd, INCFS_NAME));

    EXPECT_EQ(size_t(1), m.size());
    EXPECT_STREQ("/data/incremental/MT_data_app_vmdl703/mount",
                 m.rootFor("/data/incremental/MT_data_app_vmdl703/mount/123/2").data());
    EXPECT_STREQ("/data/incremental/MT_data_app_vmdl703/mount",
                 m.rootFor("/var/run/mount/data/mount/data/incremental/MT_data_app_vmdl703/mount/"
                           "some/thing")
                         .data());
}

TEST_F(MountRegistryTest, MultiRootLoadReversed) {
    constexpr char mountsFile[] =
            R"(4605 34 0:154 / /mnt/installer/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime shared:45 master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4561 35 0:154 / /mnt/androidwritable/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime shared:44 master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4560 99 0:154 / /storage/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,noatime master:43 - fuse /dev/fuse rw,lazytime,user_id=0,group_id=0,allow_other
4650 30 0:44 /MyFiles /mnt/pass_through/0/0000000000000000000000000000CAFEF00D2019 rw,nosuid,nodev,noexec,relatime shared:31 - 9p media rw,sync,dirsync,access=client,trans=virtio
3182 77 0:146 / /var/run/mount/data/mount/data/incremental/MT_data_app_vmdl703/mount rw,nosuid,nodev,noatime shared:46 - incremental-fs /data/incremental/MT_data_app_vmdl703/backing_store rw,seclabel,read_timeout_ms=10000,readahead=0
3181 79 0:146 / /data/incremental/MT_data_app_vmdl703/mount rw,nosuid,nodev,noatime shared:46 - incremental-fs /data/incremental/MT_data_app_vmdl703/backing_store rw,seclabel,read_timeout_ms=10000,readahead=0
)";

    TemporaryFile f;
    ASSERT_TRUE(android::base::WriteFully(f.fd, mountsFile, std::size(mountsFile) - 1));
    ASSERT_EQ(0, lseek(f.fd, 0, SEEK_SET)); // rewind

    MountRegistry::Mounts m;
    ASSERT_TRUE(m.loadFrom(f.fd, INCFS_NAME));

    EXPECT_EQ(size_t(1), m.size());
    EXPECT_STREQ("/data/incremental/MT_data_app_vmdl703/mount",
                 m.rootFor("/data/incremental/MT_data_app_vmdl703/mount/123/2").data());
    EXPECT_STREQ("/data/incremental/MT_data_app_vmdl703/mount",
                 m.rootFor("/var/run/mount/data/mount/data/incremental/MT_data_app_vmdl703/mount/"
                           "some/thing")
                         .data());
}