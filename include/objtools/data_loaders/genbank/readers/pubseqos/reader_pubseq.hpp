#ifndef READER_PUBSEQ__HPP_INCLUDED
#define READER_PUBSEQ__HPP_INCLUDED

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
*  File Description: Data reader from Pubseq_OS
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class CDB_RPCCmd;
class CDB_Result;
class I_DriverContext;

BEGIN_SCOPE(objects)


class CPubseqReader;
class CPubseqBlob;


class NCBI_XREADER_PUBSEQOS_EXPORT CPubseqReader : public CReader
{
public:
    CPubseqReader(TConn parallelLevel = 2,
                  const string& server = "PUBSEQ_OS",
                  const string& user="anyone",
                  const string& pswd = "allowed");

    ~CPubseqReader();

    virtual int ResolveSeq_id_to_gi(const CSeq_id& seqId, TConn conn);
    virtual void ResolveSeq_id(TSeqrefs& sr, const CSeq_id& id, TConn conn);
    virtual void RetrieveSeqrefs(TSeqrefs& sr, int gi, TConn conn);

    virtual CRef<CTSE_Info> GetTSEBlob(const CSeqref& seqref, TConn conn);
    virtual CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CSeqref& seqref,
                                                  TConn conn);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn);
    CDB_Connection* GetConnection(TConn);
    virtual void Reconnect(TConn);

private:
    CDB_Connection* x_GetConnection(TConn conn);
    CDB_Connection* x_NewConnection(void);


    void x_RetrieveSeqrefs(TSeqrefs& srs, CDB_RPCCmd& cmd, int gi);
    CDB_RPCCmd* x_SendRequest(const CSeqref& seqref, CDB_Connection* db_conn);
    CDB_Result* x_ReceiveData(CDB_RPCCmd& cmd);

    CRef<CTSE_Info> x_ReceiveMainBlob(CDB_Result& result);
    CRef<CSeq_annot_SNP_Info> x_ReceiveSNPAnnot(CDB_Result& result);
    
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    auto_ptr<I_DriverContext> m_Context;
    vector<CDB_Connection *>  m_Pool;
    bool                      m_NoMoreConnections;
};



END_SCOPE(objects)

extern NCBI_XREADER_PUBSEQOS_EXPORT const string kPubseqReaderDriverName;

extern "C" 
{

void NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_ReaderPubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

inline
void NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_Reader_Pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}

inline
void NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_ncbi_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}

inline void 
NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_Reader_ncbi_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}

inline void 
NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_Pubseqos_ncbi_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}

inline void 
NCBI_XREADER_PUBSEQOS_EXPORT NCBI_EntryPoint_Reader_Pubseqos_ncbi_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}

} // extern C

END_NCBI_SCOPE


/*
* $Log$
* Revision 1.28  2004/06/30 21:02:02  vasilche
* Added loading of external annotations from 26 satellite.
*
* Revision 1.27  2004/02/04 17:48:24  kuznets
* Fixed naming of entry points
*
* Revision 1.26  2004/02/04 17:44:26  kuznets
* Fixed naming of DLL entry points.
*
* Revision 1.25  2004/01/22 20:10:34  vasilche
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
* Revision 1.24  2004/01/13 21:58:42  vasilche
* Requrrected new version
*
* Revision 1.4  2004/01/13 16:55:54  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.3  2003/12/30 22:14:40  vasilche
* Updated genbank loader and readers plugins.
*
* Revision 1.22  2003/12/19 19:47:44  vasilche
* Added support for TRACE data, Seq-id ::= general { db "ti", tag id NNN }.
*
* Revision 1.21  2003/12/03 15:10:30  kuznets
* Minor bug fixed.
*
* Revision 1.20  2003/12/03 14:28:23  kuznets
* Added driver name constant.
*
* Revision 1.19  2003/12/02 16:17:42  kuznets
* Added plugin manager support for CReader interface and implementaions
* (id1 reader, pubseq reader)
*
* Revision 1.18  2003/11/26 17:55:54  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.17  2003/10/21 14:27:34  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.16  2003/09/30 16:21:59  vasilche
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
* Revision 1.15  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.14  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.13  2003/04/15 15:30:14  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.12  2003/04/15 14:24:07  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.11  2003/03/26 16:11:06  vasilche
* Removed redundant const modifier from integral return types.
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
* Revision 1.6  2002/04/11 17:40:18  kimelman
* recovery from bad commit
*
* Revision 1.5  2002/04/11 17:32:21  butanaev
* Switched to using dbapi driver manager.
*
* Revision 1.4  2002/04/10 22:47:54  kimelman
* added pubseq_reader as default one
*
* Revision 1.3  2002/04/09 18:48:14  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.2  2002/04/08 23:07:50  vakatov
* #include <vector>
* get rid of the "using ..." directive in the header
*
* Revision 1.1  2002/04/08 20:52:08  butanaev
* Added PUBSEQ reader.
*
* Revision 1.7  2002/04/05 20:53:29  butanaev
* Added code to detect database api availability.
*
* Revision 1.6  2002/04/03 18:37:33  butanaev
* Replaced DBLink with dbapi.
*
* Revision 1.5  2001/12/19 19:42:13  butanaev
* Implemented construction of PUBSEQ blob stream, CPubseqReader family  interfaces.
*
* Revision 1.4  2001/12/14 20:48:08  butanaev
* Implemented fetching Seqrefs from PUBSEQ.
*
* Revision 1.3  2001/12/13 17:50:34  butanaev
* Adjusted for new interface changes.
*
* Revision 1.2  2001/12/12 22:08:58  butanaev
* Removed duplicate code, created frame for CPubseq* implementation.
*
*/

#endif // READER_PUBSEQ__HPP_INCLUDED
