/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Eugene Vasilchenko
 *
 */

/// @file objmgr_exception.cpp
/// Implementation of object manager exception classes.
///


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/objmgr_exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const char* CObjMgrException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNotImplemented:   return "eNotImplemented";
    case eRegisterError:    return "eRegisterError";
    case eFindConflict:     return "eFindConflict";
    case eFindFailed:       return "eFindFailed";
    case eAddDataError:     return "eAddDataError";
    case eModifyDataError:  return "eModifyDataError";
    case eInvalidHandle:    return "eInvalidHandle";
    case eLockedData:       return "eLockedData";
    case eTransaction:      return "eTransaction";
    case eMissingData:      return "eMissingData";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


const char* CSeqMapException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eUnimplemented:    return "eUnimplemented";
    case eIteratorTooBig:   return "eIteratorTooBig";
    case eSegmentTypeError: return "eSegmentTypeError";
    case eDataError:        return "eSeqDataError";
    case eOutOfRange:       return "eOutOfRange";
    case eInvalidIndex:     return "eInvalidIndex";
    case eNullPointer:      return "eNullPointer";
    case eFail:             return "eFail";
    case eSelfReference:    return "eSelfReference";
    default:                return CException::GetErrCodeString();
    }
}


const char* CSeqVectorException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eCodingError:      return "eCodingError";
    case eDataError:        return "eSeqDataError";
    case eOutOfRange:       return "eOutOfRange";
    default:                return CException::GetErrCodeString();
    }
}


const char* CAnnotException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadLocation:      return "eBadLocation";
    case eFindFailed:       return "eFindFailed";
    case eLimitError:       return "eLimitError";
    case eOtherError:       return "eOtherError";
    case eIncomatibleType:  return "eIncomatibleType";
    default:                return CException::GetErrCodeString();
    }
}


const char* CLoaderException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNotImplemented:   return "eNotImplemented";
    case eNoData:           return "eNoData";
    case ePrivateData:      return "ePrivateData";
    case eConnectionFailed: return "eConnectionFailed";
    case eCompressionError: return "eCompressionError";
    case eLoaderFailed:     return "eLoaderFailed";
    case eNoConnection:     return "eNoConnection";
    case eOtherError:       return "eOtherError";
    case eRepeatAgain:      return "eRepeatAgain";
    case eBadConfig:        return "eBadConfig";
    case eNotFound:         return "eNotFound";
    default:                return CException::GetErrCodeString();
    }
}


const char* CBlobStateException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBlobStateError:  return "eBlobStateError";
    case eLoaderError:      return "eLoaderError";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


const char* CObjmgrUtilException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNotImplemented:   return "eNotImplemented";
    case eBadSequenceType:  return "eBadSequenceType";
    case eBadLocation:      return "eBadLocation";
    case eNotUnique:        return "eNotUnique";
    case eUnknownLength:    return "eUnknownLength";
    case eBadResidue:       return "eBadResidue";
    case eBadFeature:       return "eBadFeature";
    default:                return CException::GetErrCodeString();
    }
}


const char* CAnnotSearchLimitException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eTimeLimitExceded: return "eTimeLimitExceded";
    case eSegmentsLimitExceded: return "eSegmentsLimitExceded";
    default:                return CObjMgrException::GetErrCodeString();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
