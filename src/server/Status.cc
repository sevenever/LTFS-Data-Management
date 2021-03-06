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
#include "ServerIncludes.h"

Status mrStatus;

void Status::add(int reqNumber)

{
    SQLStatement stmt;
    int num;
    FsObj::file_state migState;

    std::lock_guard<std::mutex> lock(Status::mtx);

    //assert( allStates.count(reqNumber) == 0 );
    if (allStates.count(reqNumber) != 0)
        return;

    singleState state;

    stmt(Status::STATUS) << reqNumber;
    stmt.prepare();
    while (stmt.step(&migState, &num)) {
        switch (migState) {
            case FsObj::RESIDENT:
            case FsObj::TRANSFERRING:
                state.resident = num;
                break;
            case FsObj::TRANSFERRED:
                state.transferred = num;
                break;
            case FsObj::PREMIGRATED:
            case FsObj::CHANGINGFSTATE:
            case FsObj::RECALLING_PREMIG:
                state.premigrated = num;
                break;
            case FsObj::MIGRATED:
            case FsObj::RECALLING_MIG:
                state.migrated = num;
                break;
            case FsObj::FAILED:
                state.failed = num;
                break;
            default:
                TRACE(Trace::error, migState);
        }
    }
    stmt.finalize();
    allStates[reqNumber] = state;
}

void Status::remove(int reqNumber)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    allStates.erase(reqNumber);
}

void Status::updateSuccess(int reqNumber, FsObj::file_state from,
        FsObj::file_state to)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    assert(allStates.count(reqNumber) != 0);

    singleState state = allStates[reqNumber];

    switch (from) {
        case FsObj::RESIDENT:
            state.resident--;
            break;
        case FsObj::TRANSFERRED:
            state.transferred--;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated--;
            break;
        case FsObj::MIGRATED:
            state.migrated--;
            break;
        default:
            break;
    }

    switch (to) {
        case FsObj::RESIDENT:
            state.resident++;
            break;
        case FsObj::TRANSFERRED:
            state.transferred++;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated++;
            break;
        case FsObj::MIGRATED:
            state.migrated++;
            break;
        default:
            break;
    }

    allStates[reqNumber] = state;
}

void Status::updateFailed(int reqNumber, FsObj::file_state from)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    singleState state = allStates[reqNumber];

    switch (from) {
        case FsObj::RESIDENT:
            state.resident--;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated--;
            break;
        case FsObj::MIGRATED:
            state.migrated--;
            break;
        default:
            break;
    }

    state.failed++;

    allStates[reqNumber] = state;
}

void Status::get(int reqNumber, long *resident, long *transferred,
        long *premigrated, long *migrated, long *failed)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    *resident = allStates[reqNumber].resident;
    *transferred = allStates[reqNumber].transferred;
    *premigrated = allStates[reqNumber].premigrated;
    *migrated = allStates[reqNumber].migrated;
    *failed = allStates[reqNumber].failed;
}
