#include <boost/format.hpp>

#include "InternalError.h"

using namespace ltfsadmin;

std::string InternalError::GetOOBError()
{
	return "ADMINLIB_INTERNAL";
}

const char* InternalError::what(void)
{
	boost::format msg_formatter("Internal Error (%1%): [%2%:%3%] ");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	what_ = msg_formatter.str();

	return what_.c_str();
}