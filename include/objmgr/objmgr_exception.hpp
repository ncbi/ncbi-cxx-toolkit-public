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


/// Seq-loc mapper exceptions
class NCBI_XOBJMGR_EXPORT CLocMapperException : public CObjMgrException
{
public:
    enum EErrCode {
        eBadLocation,    ///< Attempt to map from/to invalid seq-loc
        eUnknownLength,  ///< Can not resolve sequence length
        eBadAlignment,   ///< Unsuported or invalid alignment
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CLocMapperException, CObjMgrException);
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
        eOtherError
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
                           (CObjMgrException::EErrCode) CException::eInvalid,
                           message, severity),
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
        eBadResidue
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CObjmgrUtilException, CObjMgrException);
};

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2006/09/21 19:18:15  vasilche
* Added check for self-references in CSeqMap_CI.
*
* Revision 1.17  2006/09/19 19:21:23  vasilche
* Added more exception types.
*
* Revision 1.16  2006/01/18 19:45:23  ssikorsk
* Added an extra argument to CException::x_Init
*
* Revision 1.15  2005/11/15 19:22:06  didenko
* Added transactions and edit commands support
*
* Revision 1.14  2005/07/14 16:58:26  vasilche
* Added CObjMgrException::eLockedData.
*
* Revision 1.13  2005/01/26 16:25:21  grichenk
* Added state flags to CBioseq_Handle.
*
* Revision 1.12  2004/12/22 15:56:11  vasilche
* Added CLoaderException::eNotImplemented.
*
* Revision 1.11  2004/12/13 15:19:20  grichenk
* Doxygenized comments
*
* Revision 1.10  2004/11/22 21:40:01  grichenk
* Doxygenized comments, replaced exception with CObjmgrUtilException.
*
* Revision 1.9  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.8  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.7  2004/03/19 14:19:08  grichenk
* Added seq-loc mapping through a seq-align.
*
* Revision 1.6  2004/03/10 16:24:27  grichenk
* Added CSeq_loc_Mapper
*
* Revision 1.5  2003/11/19 22:18:01  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.4  2003/11/17 16:03:12  grichenk
* Throw exception in CBioseq_Handle if the parent scope has been reset
*
* Revision 1.3  2003/10/22 16:12:37  vasilche
* Added CLoaderException::eNoConnection.
* Added check for 'fail' state of ID1 connection stream.
* CLoaderException::eNoConnection will be rethrown from CGBLoader.
*
* Revision 1.2  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.1  2003/09/05 17:28:32  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // OBJMGR_EXCEPTION__HPP
