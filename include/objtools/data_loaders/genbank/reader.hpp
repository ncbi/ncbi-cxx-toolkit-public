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
#include <corelib/plugin_manager.hpp>
#include <objtools/data_loaders/genbank/seqref.hpp>
#include <vector>

#define GENBANK_USE_SNP_SATELLITE_15 1

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeqref;
class CSeq_id;
class CTSE_Info;
class CSeq_annot_SNP_Info;
class CTSE_Chunk_Info;

class NCBI_XREADER_EXPORT CReader : public CObject
{
public:
    CReader(void);
    virtual ~CReader(void);

    typedef TSeqPos TPos;
    typedef unsigned TConn;

    typedef vector< CRef<CSeqref> > TSeqrefs;

    virtual int ResolveSeq_id_to_gi(const CSeq_id& id, TConn conn) = 0;
    virtual void RetrieveSeqrefs(TSeqrefs& srs, int gi, TConn conn) = 0;
    virtual void ResolveSeq_id(TSeqrefs& srs, const CSeq_id& id, TConn conn);
    virtual void PurgeSeq_id_to_gi(const CSeq_id& id);
    virtual void PurgeSeqrefs(const TSeqrefs& srs, const CSeq_id& id);

    virtual CRef<CTSE_Info> GetBlob(const CSeqref& seqref,
                                    TConn conn,
                                    CTSE_Chunk_Info* chunk_info = 0);

    virtual CRef<CTSE_Info> GetTSEBlob(const CSeqref& seqref, TConn conn) = 0;
    virtual CRef<CTSE_Info> GetSNPBlob(const CSeqref& seqref, TConn conn);

    virtual void GetTSEChunk(const CSeqref& seqref,
                             CTSE_Chunk_Info& chunk_info,
                             TConn conn);
    virtual void GetSNPChunk(const CSeqref& seqref,
                             CTSE_Chunk_Info& chunk_info,
                             TConn conn);

    virtual CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CSeqref& seqref,
                                                  TConn conn) = 0;


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
        kSNP_EntryId = 0,
        kSNP_ChunkId = 0
    };

    static bool IsSNPSeqref(const CSeqref& seqref);
    static void AddSNPSeqref(TSeqrefs& srs, int gi, int flags = 0);

    static bool s_GetEnvFlag(const char* env, bool def_val);

    static bool TrySNPSplit(void);
    static bool TrySNPTable(void);
    static bool TryStringPack(void);

    static void SetSNPReadHooks(CObjectIStream& in);
    static void SetSeqEntryReadHooks(CObjectIStream& in);
};



END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CReader,  "omreader", 1, 1, 0);

END_NCBI_SCOPE

/*
* $Log$
* Revision 1.40  2004/06/30 21:02:02  vasilche
* Added loading of external annotations from 26 satellite.
*
* Revision 1.39  2004/03/16 17:49:18  vasilche
* Use enum constant for TRACE_CHGR entries
*
* Revision 1.38  2004/02/18 13:59:57  dicuccio
* Added additional satellites
*
* Revision 1.37  2004/01/22 20:10:33  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.36  2004/01/13 16:55:52  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.35  2003/12/19 19:47:43  vasilche
* Added support for TRACE data, Seq-id ::= general { db "ti", tag id NNN }.
*
* Revision 1.34  2003/12/02 16:17:42  kuznets
* Added plugin manager support for CReader interface and implementaions
* (id1 reader, pubseq reader)
*
* Revision 1.33  2003/11/26 17:55:53  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.32  2003/10/27 18:50:48  vasilche
* Detect 'private' blobs in ID1 reader.
* Avoid reconnecting after ID1 server replied with error packet.
*
* Revision 1.31  2003/10/27 15:05:41  vasilche
* Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
* Added recognition of ID1 error codes: private, etc.
* Some formatting of old code.
*
* Revision 1.30  2003/10/08 14:16:12  vasilche
* Added version of blobs loaded from ID1.
*
* Revision 1.29  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.28  2003/09/30 16:21:59  vasilche
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
* Revision 1.27  2003/08/27 14:24:43  vasilche
* Simplified CCmpTSE class.
*
* Revision 1.26  2003/08/14 21:34:42  kans
* fixed inconsistent line endings that stopped Mac compilation
*
* Revision 1.25  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
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

