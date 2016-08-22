#ifndef _IREP_TO_SEQFEAT_ERRORS_HPP_
#define _IREP_TO_SEQFEAT_ERRORS_HPP_

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_message.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CVariationIrepException : public CException
{
public:
    enum EErrCode {
        eInvalidLocation,
        eInvalidInterval,
        eInvalidInsertion,
        eInvalidCount,
        eUnknownVariation, // Unrecognized variation type
        eInvalidVariation,  // CVarDesc data not consistent with variation type 
        eInvalidSeqType,
        eInvalidSeqId,
        eCDSError,
        eUnsupported
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CVariationIrepException,CException);
};


class CVariationValidateException : public CException
{
public: 
    enum EErrCode {
      eIDResolveError,
      eSeqliteralIntervalError,
      eInvalidType,
      eIncompleteObject
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CVariationValidateException,CException);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _IREP_TO_SEQFEAT_ERRORS_HPP_
