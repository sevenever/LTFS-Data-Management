#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoDrivesCommand.h"

void InfoDrivesCommand::printUsage()
{
	INFO(LTFSDMC0068I);
}

void InfoDrivesCommand::doCommand(int argc, char **argv)
{
	processOptions(argc, argv);

	TRACE(Trace::little, *argv);
	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);

	if( argc != optind ) {
		printUsage();
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmInfoDrivesRequest *infodrives = commCommand.mutable_infodrivesrequest();

	infodrives->set_key(key);

	try {
		commCommand.send();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	INFO(LTFSDMC0069I);

	std::string id;

	do {
		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmInfoDrivesResp infodrivesresp = commCommand.infodrivesresp();
		id = infodrivesresp.id();
		std::string devname= infodrivesresp.devname();
		unsigned long slot = infodrivesresp.slot();
		std::string status = infodrivesresp.status();
		bool busy = infodrivesresp.busy();
		if ( id.compare("") != 0 )
			INFO(LTFSDMC0070I, id, devname, slot, status, busy ? messages[LTFSDMC0071I] : messages[LTFSDMC0072I]);
	} while ( id.compare("") != 0 );

	return;
}
