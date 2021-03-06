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
#pragma once

class FileOperation
{
protected:
    unsigned long requestSize;
    static std::string genInumString(std::list<unsigned long> inumList);
public:
    static const std::string REQUEST_STATE;
    static const std::string DELETE_JOBS;
    static const std::string DELETE_REQUESTS;
    FileOperation() :
            requestSize(0)
    {
    }
    virtual ~FileOperation() = default;
    virtual void addJob(std::string fileName)
    {
    }
    virtual void start()
    {
    }
    bool queryResult(long reqNumber, long *resident, long *transferred,
            long *premigrated, long *migrated, long *failed);
    unsigned long getRequestSize()
    {
        return requestSize;
    }
};
