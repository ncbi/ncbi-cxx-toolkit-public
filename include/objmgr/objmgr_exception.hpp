#ifndef OBJMGR_EXCEPTION__HPP
#define OBJMGR_EXCEPTION__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Object manager exceptions
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// root class for all serialization exceptions
class NCBI_XOBJMGR_EXPORT CObjMgrException : public CException
{
public:
    enum EErrCode {
        eNotImplemented,
        eRegisterError,   // error while registering a data source/loader
        eFindConflict,
        eFindFailed,
        eAddDataError,
        eModifyDataError,
        eIdMapperError,
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eNotImplemented:   return "eNotImplemented";
        case eRegisterError:    return "eRegisterError";
        case eFindConflict:     return "eFindConflict";
        case eFindFailed:       return "eFindFailed";
        case eAddDataError:     return "eAddDataError";
        case eModifyDataError:  return "eModifyDataError";
        case eIdMapperError:    return "eIdMapperError";
        case eOtherError:       return "eOtherError";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CObjMgrException,CException);
};


class NCBI_XOBJMGR_EXPORT CSeqMapException : public CObjMgrException
{
public:
    enum EErrCode {
        eUnimplemented,
        eIteratorTooBig,
        eSegmentTypeError,
        eDataError,
        eOutOfRange,
        eInvalidIndex,
        eNullPointer,
        eFail
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eUnimplemented:    return "eUnimplemented";
        case eIteratorTooBig:   return "eIteratorTooBig";
        case eSegmentTypeError: return "eSegmentTypeError";
        case eDataError:        return "eSeqDataError";
        case eOutOfRange:       return "eOutOfRange";
        case eInvalidIndex:     return "eInvalidIndex";
        case eNullPointer:      return "eNullPointer";
        case eFail:             return "eFail";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSeqMapException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CSeqVectorException : public CObjMgrException
{
public:
    enum EErrCode {
        eCodingError,
        eDataError,
        eOutOfRange
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eCodingError:      return "eCodingError";
        case eDataError:        return "eSeqDataError";
        case eOutOfRange:       return "eOutOfRange";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSeqVectorException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CAnnotException : public CObjMgrException
{
public:
    enum EErrCode {
        eBadLocation,
        eFindFailed,
        eLimitError
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eBadLocation:      return "eBadLocation";
        case eFindFailed:       return "eFindFailed";
        case eLimitError:       return "eLimitError";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CAnnotException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CLoaderException : public CObjMgrException
{
public:
    enum EErrCode {
        eCompressionError,
        eLoaderFailed
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eCompressionError: return "eCompressionError";
        case eLoaderFailed:     return "eLoaderFailed";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CLoaderException, CObjMgrException);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/05 17:28:32  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // OBJMGR_EXCEPTION__HPP
