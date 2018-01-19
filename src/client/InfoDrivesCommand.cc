#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "InfoDrivesCommand.h"

/** @page ltfsdm_info_drives ltfsdm info drives
    The ltfsdm info drives command lists all available tape drives.

    <tt>@LTFSDMC0068I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info drives
    id           device name   slot         status       usage
    9068051229   /dev/IBMtape0 256          Available    free
    1013000505   /dev/IBMtape1 259          Available    free
    @endverbatim

    The corresponding class is @ref InfoDrivesCommand.
 */

void InfoDrivesCommand::printUsage()
{
    INFO(LTFSDMC0068I);
}

void InfoDrivesCommand::doCommand(int argc, char **argv)
{
    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoDrivesRequest *infodrives =
            commCommand.mutable_infodrivesrequest();

    infodrives->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0069I);

    std::string id;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoDrivesResp infodrivesresp =
                commCommand.infodrivesresp();
        id = infodrivesresp.id();
        std::string devname = infodrivesresp.devname();
        unsigned long slot = infodrivesresp.slot();
        std::string status = infodrivesresp.status();
        bool busy = infodrivesresp.busy();
        if (id.compare("") != 0)
            INFO(LTFSDMC0070I, id, devname, slot, status,
                    busy ? messages[LTFSDMC0071I] : messages[LTFSDMC0072I]);
    } while (id.compare("") != 0);

    return;
}
