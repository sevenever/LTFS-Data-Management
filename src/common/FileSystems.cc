/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <vector>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/Const.h"

#include "FileSystems.h"

FileSystems::FileSystems() :
        first(true), tb(NULL)

{
    int rc;

    cxt = mnt_new_context();

    if ((rc = blkid_get_cache(&cache, NULL)) != 0) {
        TRACE(Trace::error, rc, errno);
        THROW(Error::GENERAL_ERROR, rc, errno);
    }
}

FileSystems::~FileSystems()

{
    blkid_put_cache(cache);
    mnt_free_context(cxt);
}

void FileSystems::getTable()

{
    int rc;

    if ((rc = mnt_context_get_mtab(cxt, &tb)) != 0) {
        blkid_put_cache(cache);
        TRACE(Trace::error, rc, errno);
        THROW(Error::GENERAL_ERROR, rc, errno);
    }
}

FileSystems::fsinfo FileSystems::getContext(struct libmnt_fs *mntfs)

{
    FileSystems::fsinfo fs;
    const char *str;
    char *uuid;

    if ((str = mnt_fs_get_source(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.source = str;

    if ((str = mnt_fs_get_target(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.target = str;

    if ((str = mnt_fs_get_fstype(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.fstype = str;

    if ((str = mnt_fs_get_options(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.options = str;

    if ((uuid = blkid_get_tag_value(cache, "UUID", fs.source.c_str())) == NULL) {
        fs.uuid = "";
    } else {
        fs.uuid = uuid;
        free(uuid);
    }

    return fs;
}

FileSystems::fsinfo FileSystems::getByTarget(std::string target)

{
    struct libmnt_fs *mntfs;
    fsinfo fs;

    getTable();

    if ((mntfs = mnt_table_find_target(tb, target.c_str(), MNT_ITER_BACKWARD))
            == NULL) {
        TRACE(Trace::error, target);
        THROW(Error::GENERAL_ERROR, target);
    }

    try {
        fs = getContext(mntfs);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR, target);
    }

    return fs;
}

void FileSystems::mount(std::string source, std::string target,
        std::string options, mountflag flag)

{
    int rc;

    if ((rc = mnt_reset_context(cxt)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    getTable();

    if (flag == FileSystems::MNT_FAKE) {
        if (mnt_context_enable_fake(cxt, TRUE) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    } else {
        if (mnt_context_enable_fake(cxt, FALSE) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (source.compare("") == 0 && target.compare("") == 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if (source.compare("") != 0) {
        if ((rc = mnt_context_set_source(cxt, source.c_str())) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (target.compare("") != 0) {
        if ((rc = mnt_context_set_target(cxt, target.c_str())) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (options.compare("") != 0) {
        if ((rc = mnt_context_set_options(cxt, options.c_str())) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (mnt_context_mount(cxt) == 0)
        return;

    if ((rc = mnt_context_get_status(cxt)) != 1) {
        TRACE(Trace::error, target, rc, mnt_context_get_syscall_errno(cxt));
        THROW(Error::GENERAL_ERROR, target, rc);
    }
}

void FileSystems::umount(std::string target, umountflag flag)

{
    struct libmnt_fs *mntfs;
    fsinfo fs;
    int rc;
    int err;

    if ((rc = mnt_reset_context(cxt)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    getTable();

    if ((mntfs = mnt_table_find_target(tb, target.c_str(), MNT_ITER_BACKWARD))
            == NULL) {
        TRACE(Trace::error, target);
        THROW(Error::GENERAL_ERROR, target);
    }

    if (first == false) {
        if ((rc = mnt_reset_context(cxt)) != 0) {
            TRACE(Trace::error, target);
            THROW(Error::GENERAL_ERROR, target);
        }
    } else {
        first = true;
    }

    if (flag == FileSystems::UMNT_DETACHED
            || flag == FileSystems::UMNT_DETACHED_FORCED) {
        if ((rc = mnt_context_enable_lazy(cxt, TRUE)) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (flag == FileSystems::UMNT_FORCED
            || flag == FileSystems::UMNT_DETACHED_FORCED) {
        if ((rc = mnt_context_enable_force(cxt, TRUE)) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if ((rc = mnt_context_set_fs(cxt, mntfs)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if (mnt_context_umount(cxt) == 0)
        return;

    if ((rc = mnt_context_get_status(cxt)) != 1) {
        err = mnt_context_get_syscall_errno(cxt);
        TRACE(Trace::error, target, rc, err);
        if (err == EBUSY)
            THROW(Error::FS_BUSY, target, rc);
        else
            THROW(Error::GENERAL_ERROR, target, rc);
    }
}
