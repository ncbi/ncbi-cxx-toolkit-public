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


// root class for all object manager exceptions
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
        eInvalidHandle,
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
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
    virtual const char* GetErrCodeString(void) const;
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
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSeqVectorException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CAnnotException : public CObjMgrException
{
public:
    enum EErrCode {
        eBadLocation,
        eFindFailed,
        eLimitError,
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CAnnotException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CLocMapperException : public CObjMgrException
{
public:
    enum EErrCode {
        eBadLocation,
        eUnknownLength,
        eBadAlignment,
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CLocMapperException, CObjMgrException);
};


class NCBI_XOBJMGR_EXPORT CLoaderException : public CObjMgrException
{
public:
    enum EErrCode {
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


class NCBI_XOBJMGR_EXPORT CObjmgrUtilException : public CObjMgrException
{
public:
    enum EErrCode {
        eNotImplemented,
        eBadSequenceType,
        eBadLocation
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CObjmgrUtilException, CObjMgrException);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
