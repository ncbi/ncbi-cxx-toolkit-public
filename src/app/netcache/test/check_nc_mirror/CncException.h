#ifndef CHECK_NC_MIRROR_CNCEXCEPTION_HPP
#define CHECK_NC_MIRROR_CNCEXCEPTION_HPP

USING_NCBI_SCOPE;

#include <corelib/ncbistd.hpp>

///////////////////////////////////////////////////////////////////////

class NCBI_XNCBI_EXPORT CCncException : public CCoreException
{
public:
    enum EErrCode {
		eConfiguration,
        eConvert
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CCncException, CCoreException);
};

#endif /* CHECK_NC_MIRROR_CNCEXCEPTION_HPP */