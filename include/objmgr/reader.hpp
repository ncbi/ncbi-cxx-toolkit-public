#ifndef READER__HPP_INCLUDED
#define READER__HPP_INCLUDED
/* */

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Base data reader interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_id;
class CSeq_entry;

class NCBI_XOBJMGR_EXPORT CBlob : public CObject
{
public:
    typedef TSeqPos TPos;
    typedef int TBlobClass;
    typedef unsigned TConn;

    CBlob(void);
    virtual ~CBlob(void);

    virtual CSeq_entry *Seq_entry(void) = 0;

    string& Descr() { return m_Descr; }
    int&    Class() { return m_Class; }

private:
    int m_Class;
    string m_Descr;
};

class NCBI_XOBJMGR_EXPORT CBlobSource : public CObject
{
public:
    typedef TSeqPos TPos;
    typedef int TBlobClass;
    typedef unsigned TConn;

    CBlobSource(void);
    virtual ~CBlobSource(void);

    virtual bool HaveMoreBlobs(void) = 0;
    virtual CBlob* RetrieveBlob(void) = 0;
};

class NCBI_XOBJMGR_EXPORT CSeqref : public CObject
{
public:
    CSeqref(void);
    virtual ~CSeqref(void);
    
    typedef TSeqPos TPos;
    typedef int TBlobClass;
    typedef unsigned TConn;

    virtual CBlobSource* GetBlobSource(TPos start, TPos stop,
                                       TBlobClass blobClass,
                                       TConn conn = 0) const = 0;

    virtual const string print(void)    const = 0;
    virtual const string printTSE(void) const = 0;

    enum EMatchLevel {
        eContext,
        eTSE    ,
        eSeq
    };

    enum FFlags {
        fHasCore     = 1 << 0,
        fHasDescr    = 1 << 1,
        fHasSeqMap   = 1 << 2,
        fHasSeqData  = 1 << 3,
        fHasFeatures = 1 << 4,
        fHasExternal = 1 << 5,
        fHasAlign    = 1 << 6,
        fHasGraph    = 1 << 7,

        fHasAll      = (1 << 16)-1,
        fHasAllLocal = fHasAll & ~fHasExternal,

        fPossible    = 1 << 16
    };
    typedef int TFlags;

    virtual int Compare(const CSeqref& seqRef,
                        EMatchLevel ml = eSeq) const = 0;

    TFlags  GetFlags() const { return m_Flags; }
    void SetFlags(TFlags flags)       { m_Flags = flags; }

private:
    TFlags  m_Flags;
};

class NCBI_XOBJMGR_EXPORT CReader : public CObject
{
public:
    CReader(void);
    virtual ~CReader(void);

    typedef TSeqPos TPos;
    typedef int TBlobClass;
    typedef unsigned TConn;

    typedef vector< CRef<CSeqref> > TSeqrefs;

    virtual bool RetrieveSeqrefs(TSeqrefs& sr,
                                 const CSeq_id& seqId,
                                 TConn conn = 0) = 0;

    // return the level of reasonable parallelism
    // 1 - non MTsafe; 0 - no synchronization required,
    // n - at most n connection is advised/supported
    virtual TConn GetParallelLevel(void) const = 0;
    virtual void SetParallelLevel(TConn conn) = 0;
    virtual void Reconnect(TConn conn) = 0;

    // returns the time in secons when already retrived data
    // could become obsolete by fresher version 
    // -1 - never
    virtual int GetConst(const string& const_name) const;

    enum {
        kSat_SNP = 15
    };

    static bool s_GetEnvFlag(const char* env, bool def_val);

    static bool TrySNPSplit(void);
    static bool TryStringPack(void);

    static void SetSNPReadHooks(CObjectIStream& in);
    static void SetSeqEntryReadHooks(CObjectIStream& in);
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.24  2003/07/24 19:28:08  vasilche
* Implemented SNP split for ID1 loader.
*
* Revision 1.23  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.22  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.21  2003/04/15 15:30:14  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.20  2003/04/15 14:24:07  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.19  2003/03/26 16:11:06  vasilche
* Removed redundant const modifier from integral return types.
*
* Revision 1.18  2003/02/25 22:03:31  vasilche
* Fixed identation.
*
* Revision 1.17  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.16  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.15  2002/05/06 20:37:08  ucko
* Drop redundant CIStream:: which generated warnings under some compilers.
*
* Revision 1.14  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/08 18:37:56  ucko
* Use IOS_BASE instead of ios_base for compatibility with old compilers.
*
* Revision 1.12  2002/03/27 20:22:32  butanaev
* Added connection pool.
*
* Revision 1.11  2002/03/26 20:23:21  coremake
* fake change
*
* Revision 1.10  2002/03/26 20:19:23  coremake
* fake change
*
* Revision 1.9  2002/03/26 20:07:05  coremake
* fake change
*
* Revision 1.8  2002/03/26 19:43:01  butanaev
* Fixed compilation for g++.
*
* Revision 1.7  2002/03/26 18:48:30  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.6  2002/03/26 17:16:59  kimelman
* reader stream fixes
*
* Revision 1.5  2002/03/22 18:49:23  kimelman
* stream fix: WS skipping in binary stream
*
* Revision 1.4  2002/03/21 19:14:52  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 01:34:50  kimelman
* gbloader related bugfixes
*
* Revision 1.2  2002/03/20 04:50:35  kimelman
* GB loader added
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
* Revision 1.5  2001/12/13 00:20:55  kimelman
* bugfixes:
*
* Revision 1.4  2001/12/12 21:42:27  kimelman
* a) int -> unsigned long
* b) bool Compare -> int Compare
*
* Revision 1.3  2001/12/10 20:07:24  butanaev
* Code cleanup.
*
* Revision 1.2  2001/12/07 21:25:19  butanaev
* Interface development, code beautyfication.
*
* Revision 1.1  2001/12/07 16:11:44  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/

#endif // READER__HPP_INCLUDED
