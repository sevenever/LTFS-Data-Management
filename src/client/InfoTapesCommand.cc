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
#include "InfoTapesCommand.h"

void InfoTapesCommand::printUsage()
{
	INFO(LTFSDMC0065I);
}

void InfoTapesCommand::doCommand(int argc, char **argv)
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

	LTFSDmProtocol::LTFSDmInfoTapesRequest *infotapes = commCommand.mutable_infotapesrequest();

	infotapes->set_key(key);

	try {
		commCommand.send();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	INFO(LTFSDMC0066I);

	std::string id;

	do {
		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmInfoTapesResp infotapesresp = commCommand.infotapesresp();
		id = infotapesresp.id();
		unsigned long slot = infotapesresp.slot();
		unsigned long totalcap = infotapesresp.totalcap();
		unsigned long remaincap = infotapesresp.remaincap();
		std::string status = infotapesresp.status();
		unsigned long inprogress = infotapesresp.inprogress();
		std::string pool = infotapesresp.pool();
		std::string state = infotapesresp.state();
		if ( id.compare("") != 0 )
			INFO(LTFSDMC0067I, id, slot, totalcap, remaincap, status, inprogress, pool, state);
	} while ( id.compare("") != 0 );

	return;
}
