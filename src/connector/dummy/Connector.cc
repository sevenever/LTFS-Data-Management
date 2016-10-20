#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <string>

#include "src/connector/Connector.h"

Connector::Connector()

{
}

Connector::~Connector()

{
}

FsObj::FsObj(std::string fileName)

{
	handle = nullptr;
	handleLength = 0;
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)

{
	handle = nullptr;
	handleLength = 0;
}

FsObj::~FsObj()

{
}

struct stat FsObj::stat()

{
	struct stat statbuf;

	memset(&statbuf, 0, sizeof(statbuf));

	statbuf.st_mode = S_IFREG;

	return statbuf;
}

unsigned long long FsObj::getFsId()

{
	return 1;
}

unsigned int FsObj::getIGen()

{
	return 2;
}

unsigned long long FsObj::getINode()

{
	return 3;
}