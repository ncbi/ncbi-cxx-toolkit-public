#ifndef _PROT_MATCH_EXCEPTION_HPP_
#define _PROT_MATCH_EXCEPTION_HPP_

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CProteinMatchException : public CException {
        
public:
    enum EErrCode {
        eInputError,
        eOutputError, 
        eExecutionError,
        eBadInput,
        eNoGenomeSeq
    };

    virtual const char* GetErrCodeString() const 
    {
        switch (GetErrCode()) {
            case eInputError:
                return "eInputError";
            case eOutputError:
                return "eOutputError";
            case eExecutionError:
                 return "eExecutionError";
            case eBadInput:
                return "eBadInput";
            case eNoGenomeSeq:
                return "eNoGenomeSeq";
            default:
                return CException::GetErrCodeString();
        }
    };

    NCBI_EXCEPTION_DEFAULT(CProteinMatchException, CException);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _PROT_MATCH_EXCEPTION_
