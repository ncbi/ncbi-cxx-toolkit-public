#include <ncbi_pch.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>

// Many of these exceptions will not be triggered by the test set. 
// Exclude from code-coverage analysis.
// LCOV_EXCL_START 

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const char* CVariationIrepException::GetErrCodeString(void) const 
{
    switch( GetErrCode() ) {
        case eInvalidLocation:  return "eInvalidLocation";
        case eInvalidInterval:  return "eInvalidInterval";
        case eInvalidCount:     return "eInvalidCount";
        case eInvalidInsertion: return "eInvalidInsertion";
        case eUnknownVariation: return "eUnknownVariation";
        case eInvalidVariation: return "eInvalidVariation";
        case eInvalidSeqType:   return "eInvalidSeqType";
        case eInvalidSeqId:     return "eInvalidSeqId";
        case eUnsupported:      return "eUnsupported";
        case eCDSError:         return "eCDSError";
        default:                return  CException::GetErrCodeString();
    }
}


const char* CVariationValidateException::GetErrCodeString(void) const 
{
    switch( GetErrCode() ) {
    case eIDResolveError: return "eIDResolveError";
    case eSeqliteralIntervalError: return "eSeqliteralIntervalError";
    case eInvalidType: return "eInvalidType";
    case eIncompleteObject: return "eIncompleteObject";
    default:              return CException::GetErrCodeString();
    }
}

// LCOV_EXCL_STOP

END_SCOPE(objects)
END_NCBI_SCOPE
