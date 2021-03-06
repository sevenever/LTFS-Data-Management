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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string>
#include <fstream>
#include <list>
#include <set>
#include <sstream>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/util.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"

LTFSDMCommand::~LTFSDMCommand()

{
    try {
        std::ifstream filelist;

        if (fileListStrm.is_open()) {
            fileListStrm.close();
        }
    } catch (...) {
    }
}

void LTFSDMCommand::processOptions(int argc, char **argv)

{
    int opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, optionStr.c_str())) != -1) {
        switch (opt) {
            case 'h':
                printUsage();
                THROW(Error::OK);
            case 'p':
                preMigrate = true;
                break;
            case 'r':
                recToResident = true;
                break;
            case 'n':
                requestNumber = strtoul(optarg, NULL, 0);
                if (requestNumber < 0) {
                    MSG(LTFSDMC0064E);
                    printUsage();
                    THROW(Error::GENERAL_ERROR);
                }
                break;
            case 'f':
                fileList = optarg;
                break;
            case 'P':
                poolNames = optarg;
                break;
            case 't':
                tapeList.push_back(optarg);
                break;
            case 'x':
                forced = true;
                break;
            case 'F':
                format = true;
                break;
            case 'C':
                check = true;
                break;
            case ':':
                INFO(LTFSDMC0014E);
                printUsage();
                THROW(Error::GENERAL_ERROR);
            default:
                INFO(LTFSDMC0013E);
                printUsage();
                THROW(Error::GENERAL_ERROR);
        }
    }
}

void LTFSDMCommand::traceParms()

{
    TRACE(Trace::normal, preMigrate, recToResident, requestNumber, poolNames,
            fileList, command, optionStr, key);
}

void LTFSDMCommand::getRequestNumber()

{
    LTFSDmProtocol::LTFSDmReqNumber *reqnum = commCommand.mutable_reqnum();
    reqnum->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0028E);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmReqNumberResp reqnumresp =
            commCommand.reqnumresp();

    if (reqnumresp.success() == true) {
        requestNumber = reqnumresp.reqnumber();
        TRACE(Trace::normal, requestNumber);
    } else {
        MSG(LTFSDMC0029E);
        THROW(Error::GENERAL_ERROR);
    }
}

void LTFSDMCommand::connect()

{
    key = LTFSDM::getkey();

    try {
        commCommand.connect();
    } catch (const std::exception& e) {
        THROW(Error::GENERAL_ERROR);
    }

    if (requestNumber == Const::UNSET)
        getRequestNumber();

    commCommand.Clear();

    TRACE(Trace::normal, requestNumber);
}

void LTFSDMCommand::checkOptions(int argc, char **argv)

{
    if ((requestNumber != Const::UNSET) && (argc != 3))
        THROW(Error::GENERAL_ERROR);

    if (optind != argc) {
        if (fileList.compare("")) {
            INFO(LTFSDMC0016E);
            THROW(Error::GENERAL_ERROR);
        }
    }
}

void LTFSDMCommand::sendObjects(std::stringstream *parmList)

{
    std::istream *input;
    std::string line;
    char *file_name;
    bool cont = true;
    int i;
    long startTime;
    unsigned int count = 0;

    startTime = time(NULL);

    if (fileList.compare("")) {
        fileListStrm.open(fileList);
        input = dynamic_cast<std::istream*>(&fileListStrm);
    } else {
        input = dynamic_cast<std::istream*>(parmList);
    }

    while (cont) {
        LTFSDmProtocol::LTFSDmSendObjects *sendobjects =
                commCommand.mutable_sendobjects();
        LTFSDmProtocol::LTFSDmSendObjects::FileName* filenames;

        for (i = 0;
                (i < Const::MAX_OBJECTS_SEND) && ((std::getline(*input, line)));
                i++) {
            file_name = canonicalize_file_name(line.c_str());
            if (file_name) {
                filenames = sendobjects->add_filenames();
                filenames->set_filename(file_name);
                free(file_name);
                count++;
            } else {
                MSG(LTFSDMC0043E, line.c_str());
                not_all_exist = true;
            }
            TRACE(Trace::full, line);
        }

        if (i < Const::MAX_OBJECTS_SEND) {
            cont = false;
            filenames = sendobjects->add_filenames();
            filenames->set_filename(""); //end
            TRACE(Trace::full, "END");
        }

        try {
            commCommand.send();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0027E);
            THROW(Error::GENERAL_ERROR);
        }

        sendobjects->Clear();

        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        if (!commCommand.has_sendobjectsresp()) {
            MSG(LTFSDMC0039E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmSendObjectsResp sendobjresp =
                commCommand.sendobjectsresp();

        if (getpid() != sendobjresp.pid()) {
            MSG(LTFSDMC0036E);
            TRACE(Trace::error, getpid(), sendobjresp.pid());
            THROW(Error::GENERAL_ERROR);
        }
        if (requestNumber != sendobjresp.reqnumber()) {
            MSG(LTFSDMC0037E);
            TRACE(Trace::error, requestNumber, sendobjresp.reqnumber());
            THROW(Error::GENERAL_ERROR);
        }

        switch (sendobjresp.error()) {
            case static_cast<long>(Error::POOL_TOO_SMALL):
                MSG(LTFSDMC0015W);
                break;
            case static_cast<long>(Error::OK):
                break;
            default:
                MSG(LTFSDMC0029E);
                THROW(Error::GENERAL_ERROR);
        }
        INFO(LTFSDMC0050I, count);
    }
    INFO(LTFSDMC0051I, time(NULL) - startTime);

    commCommand.Clear();
}

void LTFSDMCommand::queryResults()

{
    bool first = true;
    bool done = false;
    time_t duration;
    struct tm tmval;
    ;
    char curctime[26];

    do {
        LTFSDmProtocol::LTFSDmReqStatusRequest *reqstatus =
                commCommand.mutable_reqstatusrequest();

        reqstatus->set_key(key);
        reqstatus->set_reqnumber(requestNumber);
        reqstatus->set_pid(getpid());

        try {
            commCommand.send();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0027E);
            THROW(Error::GENERAL_ERROR);
        }

        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmReqStatusResp reqstatusresp =
                commCommand.reqstatusresp();

        if (reqstatusresp.success() == true) {
            if (getpid() != reqstatusresp.pid()) {
                MSG(LTFSDMC0036E);
                TRACE(Trace::error, getpid(), reqstatusresp.pid());
                THROW(Error::GENERAL_ERROR);
            }
            if (requestNumber != reqstatusresp.reqnumber()) {
                MSG(LTFSDMC0037E);
                TRACE(Trace::error, requestNumber, reqstatusresp.reqnumber());
                THROW(Error::GENERAL_ERROR);
            }
            resident = reqstatusresp.resident();
            transferred = reqstatusresp.transferred();
            premigrated = reqstatusresp.premigrated();
            migrated = reqstatusresp.migrated();
            failed = reqstatusresp.failed();
            done = reqstatusresp.done();

            duration = time(NULL) - startTime;
            gmtime_r(&duration, &tmval);
            strftime(curctime, sizeof(curctime) - 1, "%H:%M:%S", &tmval);
            if (first) {
                INFO(LTFSDMC0046I);
                first = false;
            }
            INFO(LTFSDMC0045I, curctime, resident, transferred, premigrated,
                    migrated, failed);
        } else {
            MSG(LTFSDMC0029E);
            THROW(Error::GENERAL_ERROR);
        }
    } while (!done);

    commCommand.Clear();
}

void LTFSDMCommand::isValidRegularFile()

{
    struct stat statbuf;

    if (!fileList.compare("-")) { // if "-" is presented read from stdin
        fileList = "/dev/stdin";
    } else if (fileList.compare("")) {
        if (stat(fileList.c_str(), &statbuf) == -1) {
            MSG(LTFSDMC0040E, fileList.c_str());
            THROW(Error::GENERAL_ERROR);
        }
        if (!S_ISREG(statbuf.st_mode)) {
            MSG(LTFSDMC0042E, fileList.c_str());
            THROW(Error::GENERAL_ERROR);
        }
        if (statbuf.st_size < 2) {
            MSG(LTFSDMC0041E, fileList.c_str());
            THROW(Error::GENERAL_ERROR);
        }
    }
}
