#include "ncbi_pch.hpp"
#include "CncException.h"

///////////////////////////////////////////////////////////////////////

const char* CCncException::GetErrCodeString(void) const
{
	switch (GetErrCode()) {
		case eConfiguration: 
			return "eConfiguration";
		case eConvert: 
			return "eConvert";
		default:
			return CException::GetErrCodeString();
	}
}