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


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


/// Base class for all object manager exceptions
class NCBI_XOBJMGR_EXPORT CObjMgrException : public CException
{
public:
    enum EErrCode {
        eNotImplemented,  ///< The method is not implemented
        eRegisterError,   ///< Error while registering a data source/loader
        eFindConflict,    ///< Conflicting data found
        eFindFailed,      ///< The data requested can not be found
        eAddDataError,    ///< Error while adding new data
        eModifyDataError, ///< Error while modifying data
        eInvalidHandle,   ///< Attempt to use an invalid handle
        eLockedData,      ///< Attempt to remove locked data
        eTransaction,     ///< Transaction violation
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CObjMgrException,CException);
};


/// SeqMap related exceptions
class NCBI_XOBJMGR_EXPORT CSeqMapException : public CObjMgrException
{
public:
    enum EErrCode {
        eUnimplemented,    ///< The method is not implemented
        eIteratorTooBig,   ///< Bad internal iterator in delta map
        eSegmentTypeError, ///< Wrong segment type
        eDataError,        ///< SeqMap data error
        eOutOfRange,       ///< Iterator is out of range
        eInvalidIndex,     ///< Invalid segment index
        eNullPointer,      ///< Attempt to access non-existing object
        eSelfReference,    ///< Self-reference in seq map is detected
        eFail              ///< Operation failed
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSeqMapException, CObjMgrException);
};


/// SeqVector related exceptions
class NCBI_XOBJMGR_EXPORT CSeqVectorException : public CObjMgrException
{
public:
    enum EErrCode {
        eCodingError,   ///< Incompatible coding selected
        eDataError,     ///< Sequence data error
        eOutOfRange     ///< Attempt to access out-of-range iterator
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSeqVectorException, CObjMgrException);
};


/// Annotation iterators exceptions
class NCBI_XOBJMGR_EXPORT CAnnotException : public CObjMgrException
{
public:
    enum EErrCode {
        eBadLocation,  ///< Wrong location type while mapping annotations
        eFindFailed,   ///< Seq-id can not be resolved
        eLimitError,   ///< Invalid or unknown limit object
        eIncomatibleType, ///< Incompatible annotation type (feat/graph/align)
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CAnnotException, CObjMgrException);
};


/// Data loader exceptions, used by GenBank loader.
class NCBI_XOBJMGR_EXPORT CLoaderException : public CObjMgrException
{
public:
    enum EErrCode {
        eNotImplemented,
        eNoData,
        ePrivateData,
        eConnectionFailed,
        eCompressionError,
        eLoaderFailed,
        eNoConnection,
        eOtherError,
        eRepeatAgain,
        eBadConfig
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CLoaderException, CObjMgrException);
};


/// Blob state exceptions, used by GenBank loader.
class NCBI_XOBJMGR_EXPORT CBlobStateException : public CObjMgrException
{
public:
    enum EErrCode {
        eBlobStateError,
        eLoaderError,
        eOtherError
    };
    typedef int TBlobState;

    virtual const char* GetErrCodeString(void) const;
    CBlobStateException(const CDiagCompileInfo& info,
                        const CException* prev_exception,
                        EErrCode err_code,
                        const string& message,
                        TBlobState state,
                        EDiagSev severity = eDiag_Error)
        : CObjMgrException(info, prev_exception,
                           message, severity, 0),
          m_BlobState(state)
    {
        x_Init(info, message, prev_exception, severity);
        x_InitErrCode((CException::EErrCode) err_code);
    }
    CBlobStateException(const CBlobStateException& other)
        : CObjMgrException(other),
          m_BlobState(other.m_BlobState)
    {
        x_Assign(other);
    }
    virtual ~CBlobStateException(void) throw() {}
    virtual const char* GetType(void) const { return "CBlobStateException"; }
    typedef int TErrCode;
    TErrCode GetErrCode(void) const
    {
        return typeid(*this) == typeid(CBlobStateException) ?
            (TErrCode)x_GetErrCode() : (TErrCode)CException::eInvalid;
    }
    TBlobState GetBlobState(void)
    {
        return m_BlobState;
    }

protected:
    CBlobStateException(void) {}
    virtual const CException* x_Clone(void) const
    {
        return new CBlobStateException(*this);
    }
private:
    TBlobState m_BlobState;
};


/// Exceptions for objmgr/util library.
class NCBI_XOBJMGR_EXPORT CObjmgrUtilException : public CObjMgrException
{
public:
    enum EErrCode {
        eNotImplemented,
        eBadSequenceType,
        eBadLocation,
        eNotUnique,
        eUnknownLength,
        eBadFeature,
        eBadResidue
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CObjmgrUtilException, CObjMgrException);
};


/////////////////////////////////////////////////////////////////////////////
//  CAnnotSearchLimitException::
//
//    Exception used to notify that some annot iterator search limit
//    is reached, aborting subsequent search.
//

class NCBI_XOBJMGR_EXPORT CAnnotSearchLimitException : public CObjMgrException
{
public:
    enum EErrCode {
        eTimeLimitExceded = CObjMgrException::eOtherError + 1,
        eSegmentsLimitExceded
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CAnnotSearchLimitException, CObjMgrException);
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJMGR_EXCEPTION__HPP
