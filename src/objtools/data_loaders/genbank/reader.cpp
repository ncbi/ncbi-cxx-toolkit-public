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

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>

#include <serial/pack_string.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static const char* const STRING_PACK_ENV = "GENBANK_SNP_PACK_STRINGS";
static const char* const SNP_SPLIT_ENV = "GENBANK_SNP_SPLIT";
static const char* const SNP_TABLE_ENV = "GENBANK_SNP_TABLE";
static const char* const ENV_YES = "YES";

CReader::CReader(void)
{
}


CReader::~CReader(void)
{
}


int CReader::GetConst(const string& ) const
{
    return 0;
}


bool CReader::s_GetEnvFlag(const char* env, bool def_val)
{
    const char* val = ::getenv(env);
    if ( !val ) {
        return def_val;
    }
    string s(val);
    return s == "1" || NStr::CompareNocase(s, ENV_YES) == 0;
}


bool CReader::TrySNPSplit(void)
{
    static bool snp_split = s_GetEnvFlag(SNP_SPLIT_ENV, true);
    return snp_split;
}


bool CReader::TrySNPTable(void)
{
    static bool snp_table = s_GetEnvFlag(SNP_TABLE_ENV, true);
    return snp_table;
}


bool CReader::TryStringPack(void)
{
    static bool use_string_pack =
        CPackString::TryStringPack() && s_GetEnvFlag(STRING_PACK_ENV, true);
    return use_string_pack;
}


void CReader::SetSNPReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
    type.FindMember("val").SetLocalReadHook(in,
                                            new CPackStringClassHook(4, 128));

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CObjectTypeInfo(CType<CSeq_feat>());
    type.FindMember("comment").SetLocalReadHook(in, new CPackStringClassHook);
}


void CReader::SetSeqEntryReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
}


bool CReader::IsSNPSeqref(const CSeqref& seqref)
{
    return seqref.GetSat() == eSatellite_SNP;
}


void CReader::AddSNPSeqref(TSeqrefs& srs, int gi, CSeqref::TFlags flags)
{
    flags |= CSeqref::fHasExternal;
    CRef<CSeqref> sr(new CSeqref(gi, eSatellite_SNP, gi));
    sr->SetFlags(flags);
    srs.push_back(sr);
}


void CReader::ResolveSeq_id(TSeqrefs& srs, const CSeq_id& id, TConn conn)
{
    int gi;
    if ( id.IsGi() ) {
        gi = id.GetGi();
    }
    else {
        gi = ResolveSeq_id_to_gi(id, conn);
    }
    if ( gi ) {
        RetrieveSeqrefs(srs, gi, conn);
    }
}


void CReader::PurgeSeq_id_to_gi(const CSeq_id& /*id*/)
{
}


void CReader::PurgeSeqrefs(const TSeqrefs& /*srs*/,
                           const CSeq_id& /*id*/)
{
}


CRef<CTSE_Info> CReader::GetBlob(const CSeqref& seqref,
                                 TConn conn,
                                 CTSE_Chunk_Info* chunk_info)
{
    CRef<CTSE_Info> ret;
    if ( chunk_info ) {
        if ( IsSNPSeqref(seqref) && chunk_info->GetChunkId()==kSNP_ChunkId ) {
            GetSNPChunk(seqref, *chunk_info, conn);
        }
        else {
            GetTSEChunk(seqref, *chunk_info, conn);
        }
    }
    else {
        if ( IsSNPSeqref(seqref) ) {
            ret = GetSNPBlob(seqref, conn);
        }
        else {
            ret = GetTSEBlob(seqref, conn);
        }
    }
    return ret;
}


CRef<CTSE_Info> CReader::GetSNPBlob(const CSeqref& seqref, TConn /*conn*/)
{
    _ASSERT(IsSNPSeqref(seqref));
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    seq_entry->SetSet().SetSeq_set();
    seq_entry->SetSet().SetId().SetId(kSNP_EntryId);

    // create CTSE_Info
    CRef<CTSE_Info> ret(new CTSE_Info(*seq_entry));
    ret->SetName("SNP");

    CRef<CTSE_Chunk_Info> info(new CTSE_Chunk_Info(kSNP_ChunkId));

    info->x_AddAnnotPlace(CTSE_Chunk_Info::eBioseq_set, kSNP_EntryId);

    info->x_AddAnnotType(CAnnotName("SNP"),
                         SAnnotTypeSelector(CSeqFeatData::eSubtype_variation),
                         CSeq_id_Handle::GetGiHandle(seqref.GetGi()),
                         CTSE_Chunk_Info::TLocationRange::GetWhole());

    info->x_TSEAttach(*ret);
    return ret;
}


void CReader::GetTSEChunk(const CSeqref& /*seqref*/,
                          CTSE_Chunk_Info& /*chunk_info*/,
                          TConn /*conn*/)
{
    NCBI_THROW(CLoaderException, eNoData,
               "Chunks are not implemented");
}


void CReader::GetSNPChunk(const CSeqref& seqref,
                          CTSE_Chunk_Info& chunk,
                          TConn conn)
{
    _ASSERT(IsSNPSeqref(seqref));
    _ASSERT(chunk.GetChunkId() == kSNP_ChunkId);
    CRef<CSeq_annot_SNP_Info> snp_annot = GetSNPAnnot(seqref, conn);
    CRef<CSeq_annot_Info> annot_info(new CSeq_annot_Info(*snp_annot));
    CTSE_Chunk_Info::TPlace place(CTSE_Chunk_Info::eBioseq_set, kSNP_EntryId);
    chunk.x_LoadAnnot(place, annot_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.34  2004/06/15 14:08:22  vasilche
 * Added parsing split info with split sequences.
 *
 * Revision 1.33  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.32  2004/03/16 15:47:29  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.31  2004/02/18 14:01:25  dicuccio
 * Added new satellites for TRACE_ASSM, TR_ASSM_CH.  Added support for overloading
 * the ID1 named service
 *
 * Revision 1.30  2004/02/17 21:18:53  vasilche
 * Fixed 'unused argument' warnings.
 *
 * Revision 1.29  2004/01/22 20:10:35  vasilche
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
 * Revision 1.28  2004/01/13 16:55:55  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.27  2003/11/28 17:53:15  vasilche
 * Avoid calling CStreamUtils::Pushback() when constructing objects from text ASN.
 *
 * Revision 1.26  2003/11/26 17:55:58  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.25  2003/10/27 15:05:41  vasilche
 * Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
 * Added recognition of ID1 error codes: private, etc.
 * Some formatting of old code.
 *
 * Revision 1.24  2003/10/08 14:16:13  vasilche
 * Added version of blobs loaded from ID1.
 *
 * Revision 1.23  2003/10/07 13:43:23  vasilche
 * Added proper handling of named Seq-annots.
 * Added feature search from named Seq-annots.
 * Added configurable adaptive annotation search (default: gene, cds, mrna).
 * Fixed selection of blobs for loading from GenBank.
 * Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
 * Fixed leaked split chunks annotation stubs.
 * Moved some classes definitions in separate *.cpp files.
 *
 * Revision 1.22  2003/09/30 16:22:02  vasilche
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
 * Revision 1.21  2003/08/27 14:25:22  vasilche
 * Simplified CCmpTSE class.
 *
 * Revision 1.20  2003/08/19 18:35:21  vasilche
 * CPackString classes were moved to SERIAL library.
 *
 * Revision 1.19  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 * Revision 1.18  2003/07/24 19:28:09  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * Revision 1.17  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.16  2003/06/02 16:06:38  dicuccio
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
 * Revision 1.15  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.14  2003/04/15 16:25:39  vasilche
 * Added initialization of int members.
 *
 * Revision 1.13  2003/04/15 14:24:08  vasilche
 * Changed CReader interface to not to use fake streams.
 *
 * Revision 1.12  2003/03/28 03:27:24  lavr
 * CIStream::Eof() conditional compilation removed; code reformatted
 *
 * Revision 1.11  2003/03/26 22:12:11  lavr
 * Revert CIStream::Eof() to destructive test
 *
 * Revision 1.10  2003/03/26 20:42:50  lavr
 * CIStream::Eof() made (temporarily) non-destructive w/o get()
 *
 * Revision 1.9  2003/02/26 18:02:39  vasilche
 * Added istream error check.
 * Avoid use of string::c_str() method.
 *
 * Revision 1.8  2003/02/25 22:03:44  vasilche
 * Fixed identation.
 *
 * Revision 1.7  2002/11/27 21:09:43  lavr
 * Take advantage of CStreamUtils::Readsome() in CIStream::Read()
 * CIStream::Eof() modified to use get() instead of operator>>()
 *
 * Revision 1.6  2002/05/06 03:28:47  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.5  2002/03/27 20:23:50  butanaev
 * Added connection pool.
 *
 * Revision 1.4  2002/03/27 18:06:08  kimelman
 * stream.read/write instead of << >>
 *
 * Revision 1.3  2002/03/21 19:14:54  kimelman
 * GB related bugfixes
 *
 * Revision 1.2  2002/03/20 04:50:13  kimelman
 * GB loader added
 *
 * Revision 1.1  2002/01/11 19:06:21  gouriano
 * restructured objmgr
 *
 * Revision 1.6  2001/12/13 00:19:25  kimelman
 * bugfixes:
 *
 * Revision 1.5  2001/12/12 21:46:40  kimelman
 * Compare interface fix
 *
 * Revision 1.4  2001/12/10 20:08:01  butanaev
 * Code cleanup.
 *
 * Revision 1.3  2001/12/07 21:24:59  butanaev
 * Interface development, code beautyfication.
 *
 * Revision 1.2  2001/12/07 16:43:58  butanaev
 * Fixed includes.
 *
 * Revision 1.1  2001/12/07 16:10:22  butanaev
 * Switching to new reader interfaces.
 *
 * Revision 1.2  2001/12/06 18:06:22  butanaev
 * Ported to linux.
 *
 * Revision 1.1  2001/12/06 14:35:22  butanaev
 * New streamable interfaces designed, ID1 reimplemented.
 *
 */
