#ifndef READER_ID1__HPP_INCLUDED
#define READER_ID1__HPP_INCLUDED

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
*  File Description: Data reader from ID1
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>

BEGIN_NCBI_SCOPE

class CConn_ServiceStream;
class CByteSourceReader;
class CObjectIStream;

BEGIN_SCOPE(objects)

class CID1server_back;
class CID1server_request;
class CID1server_maxcomplex;
class CID1blob_info;
class CID2S_Split_Info;

class NCBI_XREADER_ID1_EXPORT CId1Reader : public CReader
{
public:
    CId1Reader(TConn noConn = 5);
    ~CId1Reader();

    virtual int ResolveSeq_id_to_gi(const CSeq_id& seqId, TConn conn);
    virtual void ResolveSeq_id(TSeqrefs& sr, const CSeq_id& id, TConn conn);
    virtual void RetrieveSeqrefs(TSeqrefs& sr, int gi, TConn conn);

    CRef<CTSE_Info> GetTSEBlob(const CSeqref& seqref, TConn conn);
    CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CSeqref& seqref, TConn conn);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn noConn);
    virtual void Reconnect(TConn conn);

    int GetVersion(const CSeqref& seqref, TConn conn);

protected:
    CConn_ServiceStream* x_GetConnection(TConn conn);
    CConn_ServiceStream* x_NewConnection(void);

    static int CollectStatistics(void); // 0 - no stats, >1 - verbose
    void PrintStatistics(void) const;
    static void PrintStat(const char* type,
                          size_t count, const char* what, double time);
    static void PrintBlobStat(const char* type,
                              size_t count, double bytes, double time);
    static void LogStat(const char* type, const string& name, double time);
    static void LogStat(const char* type,
                        const string& name, const string& subkey, double time);
    static void LogStat(const char* type, const CSeq_id& id, double time);
    static void LogStat(const char* type,
                        const CID1server_maxcomplex& maxplex, double time);
    static void LogStat(const char* type, int gi, double time);
    static void LogBlobStat(const char* type,
                            const CSeqref& seqref, double bytes, double time);

    virtual int x_GetVersion(const CSeqref& seqref, TConn conn);


    virtual void x_GetTSEBlob(CID1server_back& id1_reply,
                              CRef<CID2S_Split_Info>& split_info,
                              const CSeqref&   seqref,
                              TConn            conn);
    virtual void x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                               const CSeqref&       seqref,
                               TConn                conn);

    virtual void x_ReadTSEBlob(CID1server_back& id1_reply,
                               const CSeqref&   seqref,
                               CNcbiIstream&    stream);
    void x_ReadTSEBlob(CID1server_back& id1_reply,
                       CObjectIStream& stream);
    virtual void x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                const CSeqref&       seqref,
                                CByteSourceReader&   reader);

    void x_ResolveId(CID1server_back& id1_reply,
                     const CID1server_request& id1_request,
                     TConn conn);

    void x_SendRequest(const CSeqref& seqref,
                       CConn_ServiceStream* stream);
    void x_SetParams(const CSeqref& seqref,
                     CID1server_maxcomplex& params);

    int x_GetVersion(const CID1blob_info& info) const;

private:

    CRef<CTSE_Info> x_ReceiveMainBlob(CConn_ServiceStream* stream);

    vector<CConn_ServiceStream *> m_Pool;
    bool                          m_NoMoreConnections;
};


END_SCOPE(objects)

extern NCBI_XREADER_ID1_EXPORT const string kId1ReaderDriverName;

extern "C" 
{

void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_Id1Reader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

inline 
void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_Reader_Id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}

inline 
void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_ncbi_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}

inline 
void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_Reader_ncbi_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}

inline 
void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_Reader_Id1_ncbi_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}

inline 
void NCBI_XREADER_ID1_EXPORT NCBI_EntryPoint_Id1_ncbi_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}

} // extern C

END_NCBI_SCOPE


/*
* $Log$
* Revision 1.35  2004/06/30 21:02:02  vasilche
* Added loading of external annotations from 26 satellite.
*
* Revision 1.34  2004/02/04 17:44:01  kuznets
* Fixed naming of DLL entry points.
*
* Revision 1.33  2004/01/22 20:10:34  vasilche
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
* Revision 1.32  2004/01/13 21:58:42  vasilche
* Requrrected new version
*
* Revision 1.4  2004/01/13 16:55:53  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.3  2003/12/30 22:14:40  vasilche
* Updated genbank loader and readers plugins.
*
* Revision 1.30  2003/12/30 16:00:05  vasilche
* Added support for new ICache (CBDB_Cache) interface.
*
* Revision 1.29  2003/12/19 19:47:44  vasilche
* Added support for TRACE data, Seq-id ::= general { db "ti", tag id NNN }.
*
* Revision 1.28  2003/12/03 14:28:22  kuznets
* Added driver name constant.
*
* Revision 1.27  2003/12/02 16:17:42  kuznets
* Added plugin manager support for CReader interface and implementaions
* (id1 reader, pubseq reader)
*
* Revision 1.26  2003/11/26 17:55:53  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.25  2003/10/24 13:27:40  vasilche
* Cached ID1 reader made more safe. Process errors and exceptions correctly.
* Cleaned statistics printing methods.
*
* Revision 1.24  2003/10/21 16:32:50  vasilche
* Cleaned ID1 statistics messages.
* Now by setting GENBANK_ID1_STATS=1 CId1Reader collects and displays stats.
* And by setting GENBANK_ID1_STATS=2 CId1Reader logs all activities.
*
* Revision 1.23  2003/10/21 14:27:34  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.22  2003/10/14 18:31:53  vasilche
* Added caching support for SNP blobs.
* Added statistics collection of ID1 connection.
*
* Revision 1.21  2003/10/08 14:16:12  vasilche
* Added version of blobs loaded from ID1.
*
* Revision 1.20  2003/10/01 18:06:58  kuznets
* x_ReceiveSNPAnnot made public
*
* Revision 1.19  2003/10/01 16:54:50  kuznets
* Relaxed scope restriction of CId1Reader::x_SendRequest() (private->protected)
*
* Revision 1.18  2003/09/30 19:38:26  vasilche
* Added support for cached id1 reader.
*
* Revision 1.17  2003/09/30 16:21:59  vasilche
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
* Revision 1.16  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.15  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/04/15 14:24:07  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.13  2003/03/26 16:11:06  vasilche
* Removed redundant const modifier from integral return types.
*
* Revision 1.12  2003/03/01 22:26:07  kimelman
* performance fixes
*
* Revision 1.11  2003/02/04 16:02:22  dicuccio
* Moved headers unecessarily included here into the .cpp file
*
* Revision 1.10  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/03/27 20:22:32  butanaev
* Added connection pool.
*
* Revision 1.5  2002/03/26 18:48:31  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.4  2002/03/21 19:14:52  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 01:34:51  kimelman
* gbloader related bugfixes
*
* Revision 1.2  2002/03/20 04:50:35  kimelman
* GB loader added
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
* Revision 1.6  2001/12/13 00:20:55  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:43:47  kimelman
* more meaningfull Compare
*
* Revision 1.4  2001/12/10 20:07:24  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:25:19  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 17:02:06  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:11:45  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/

#endif // READER_ID1__HPP_INCLUDED
