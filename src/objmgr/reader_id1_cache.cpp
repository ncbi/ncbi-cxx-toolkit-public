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
 *
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
 *
 *  File Description: Cached extension of data reader from ID1
 *
 */

#include <objmgr/reader_id1_cache.hpp>

#include <util/cache/blob_cache.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CCachedId1Reader::CCachedId1Reader(TConn noConn)
    : CId1Reader(noConn)
{
}


CCachedId1Reader::~CCachedId1Reader()
{
}


void CCachedId1Reader::x_ReadBlob(CID1server_back& id1_reply,
                                  const CSeqref& seqref,
                                  TConn conn)
{
    auto_ptr<IReader> reader(OpenBlob(seqref));
    if ( reader.get() ) {
        CIReaderByteSourceReader rd(*reader);
        CObjectIStreamAsnBinary in(rd);
        CReader::SetSeqEntryReadHooks(in);
        in >> id1_reply;
    }
    else {
        CId1Reader::x_ReadBlob(id1_reply, seqref, conn);
    }
}


void CCachedId1Reader::x_ReadBlob(CID1server_back& id1_reply,
                                  const CSeqref& seqref,
                                  CNcbiIstream& stream)
{
    auto_ptr<IWriter> writer(StoreBlob(seqref));
    if ( writer.get() ) {
        try {
            CProxyByteSourceReader proxy(stream, *writer);
            CObjectIStreamAsnBinary in(proxy);
            
            CReader::SetSeqEntryReadHooks(in);
            
            CStreamDelayBufferGuard guard(in);
            
            in >> id1_reply;
        }
        catch ( ... ) {
            writer->Erase();
            throw;
        }
    }
    else {
        CId1Reader::x_ReadBlob(id1_reply, seqref, stream);
    }
}


CRef<CSeq_annot_SNP_Info> CCachedId1Reader::GetSNPAnnot(const CSeqref& seqref,
                                                        TConn conn)
{
    CConn_ServiceStream* stream = x_GetConnection(conn);
    x_SendRequest(seqref, stream, true);
    return x_ReceiveSNPAnnot(stream);
}


CRef<CSeq_annot_SNP_Info>
CId1Reader::x_ReceiveSNPAnnot(CConn_ServiceStream* stream)
{
    CRef<CSeq_annot_SNP_Info> snp_annot_info(new CSeq_annot_SNP_Info);

    {{
        const size_t kSkipHeader = 2, kSkipFooter = 2;
        
        CStreamByteSourceReader src(0, stream);
        
        s_SkipBytes(src, kSkipHeader);
        
        CResultZBtSrc src2(&src);
        
        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Create(eSerial_AsnBinary, src2));
        
        snp_annot_info->Read(*in);
        
        s_SkipBytes(src, kSkipFooter);
    }}

    return snp_annot_info;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.1  2003/09/30 19:38:26  vasilche
 * Added support for cached id1 reader.
 *
 * Revision 1.44  2003/09/30 16:22:02  vasilche
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
 * Revision 1.43  2003/08/27 14:25:22  vasilche
 * Simplified CCmpTSE class.
 *
 * Revision 1.42  2003/08/19 18:34:40  vasilche
 * Fixed warning about enum conversion.
 *
 * Revision 1.41  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 * Revision 1.40  2003/07/24 19:28:09  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * Revision 1.39  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.38  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.37  2003/05/13 20:14:40  vasilche
 * Catching exceptions and reconnection were moved from readers to genbank loader.
 *
 * Revision 1.36  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.35  2003/04/15 15:30:15  vasilche
 * Added include <memory> when needed.
 * Removed buggy buffer in printing methods.
 * Removed unnecessary include of stream_util.hpp.
 *
 * Revision 1.34  2003/04/15 14:24:08  vasilche
 * Changed CReader interface to not to use fake streams.
 *
 * Revision 1.33  2003/04/07 16:56:57  vasilche
 * Fixed unassigned member in ID1 request.
 *
 * Revision 1.32  2003/03/31 17:02:03  lavr
 * Some code reformatting to [more closely] meet coding requirements
 *
 * Revision 1.31  2003/03/30 07:00:29  lavr
 * MIPS-specific workaround for lame-designed stream read ops
 *
 * Revision 1.30  2003/03/28 03:28:14  lavr
 * CId1Reader::xsgetn() added; code heavily reformatted
 *
 * Revision 1.29  2003/03/27 19:38:13  vasilche
 * Use safe NStr::IntToString() instead of sprintf().
 *
 * Revision 1.28  2003/03/03 20:34:51  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.27  2003/03/01 22:26:56  kimelman
 * performance fixes
 *
 * Revision 1.26  2003/02/04 18:53:36  dicuccio
 * Include file clean-up
 *
 * Revision 1.25  2002/12/26 20:53:25  dicuccio
 * Minor tweaks to relieve compiler warnings in MSVC
 *
 * Revision 1.24  2002/12/26 16:39:24  vasilche
 * Object manager class CSeqMap rewritten.
 *
 * Revision 1.23  2002/11/18 19:48:43  grichenk
 * Removed "const" from datatool-generated setters
 *
 * Revision 1.22  2002/07/25 15:01:51  grichenk
 * Replaced non-const GetXXX() with SetXXX()
 *
 * Revision 1.21  2002/05/09 21:40:59  kimelman
 * MT tuning
 *
 * Revision 1.20  2002/05/06 03:28:47  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.19  2002/05/03 21:28:10  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.18  2002/04/17 19:52:02  kimelman
 * fix: no gi found
 *
 * Revision 1.17  2002/04/09 16:10:56  ucko
 * Split CStrStreamBuf out into a common location.
 *
 * Revision 1.16  2002/04/08 20:52:26  butanaev
 * Added PUBSEQ reader.
 *
 * Revision 1.15  2002/03/29 02:47:05  kimelman
 * gbloader: MT scalability fixes
 *
 * Revision 1.14  2002/03/27 20:23:50  butanaev
 * Added connection pool.
 *
 * Revision 1.13  2002/03/26 23:31:08  gouriano
 * memory leaks and garbage collector fix
 *
 * Revision 1.12  2002/03/26 18:48:58  butanaev
 * Fixed bug not deleting streambuf.
 *
 * Revision 1.11  2002/03/26 17:17:02  kimelman
 * reader stream fixes
 *
 * Revision 1.10  2002/03/26 15:39:25  kimelman
 * GC fixes
 *
 * Revision 1.9  2002/03/25 18:12:48  grichenk
 * Fixed bool convertion
 *
 * Revision 1.8  2002/03/25 17:49:13  kimelman
 * ID1 failure handling
 *
 * Revision 1.7  2002/03/25 15:44:47  kimelman
 * proper logging and exception handling
 *
 * Revision 1.6  2002/03/22 21:50:21  kimelman
 * bugfix: avoid history rtequest for nonexistent sequence
 *
 * Revision 1.5  2002/03/21 19:14:54  kimelman
 * GB related bugfixes
 *
 * Revision 1.4  2002/03/21 01:34:55  kimelman
 * gbloader related bugfixes
 *
 * Revision 1.3  2002/03/20 04:50:13  kimelman
 * GB loader added
 *
 * Revision 1.2  2002/01/16 18:56:28  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 1.1  2002/01/11 19:06:22  gouriano
 * restructured objmgr
 *
 * Revision 1.7  2001/12/17 21:38:24  butanaev
 * Code cleanup.
 *
 * Revision 1.6  2001/12/13 00:19:25  kimelman
 * bugfixes:
 *
 * Revision 1.5  2001/12/12 21:46:40  kimelman
 * Compare interface fix
 *
 * Revision 1.4  2001/12/10 20:08:02  butanaev
 * Code cleanup.
 *
 * Revision 1.3  2001/12/07 21:24:59  butanaev
 * Interface development, code beautyfication.
 *
 * Revision 1.2  2001/12/07 16:43:58  butanaev
 * Fixed includes.
 *
 * Revision 1.1  2001/12/07 16:10:23  butanaev
 * Switching to new reader interfaces.
 *
 * Revision 1.3  2001/12/06 20:37:05  butanaev
 * Fixed timeout problem.
 *
 * Revision 1.2  2001/12/06 18:06:22  butanaev
 * Ported to linux.
 *
 * Revision 1.1  2001/12/06 14:35:23  butanaev
 * New streamable interfaces designed, ID1 reimplemented.
 *
 */
