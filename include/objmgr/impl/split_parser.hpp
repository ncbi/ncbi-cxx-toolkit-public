#ifndef ID2_PARSER__HPP_INCLUDED
#define ID2_PARSER__HPP_INCLUDED
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
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Methods to create object manager structures from ID2 spec
 *
 */

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <vector>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CID2S_Split_Info;
class CID2S_Chunk_Info;
class CID2S_Chunk;
class CID2S_Seq_descr_Info;
class CID2S_Seq_annot_Info;
class CID2S_Seq_annot_place_Info;
class CID2S_Bioseq_place_Info;
class CID2S_Seq_data_Info;
class CID2S_Seq_loc;
class CID2S_Seq_assembly_Info;
class CID2S_Seq_feat_Ids_Info;

class CTSE_Info;
class CTSE_Chunk_Info;
class CSeq_id_Handle;

class NCBI_XOBJMGR_EXPORT CSplitParser
{
public:
    static void Attach(CTSE_Info& tse, const CID2S_Split_Info& split);

    static CRef<CTSE_Chunk_Info> Parse(const CID2S_Chunk_Info& info);

    static void Load(CTSE_Chunk_Info& chunk, const CID2S_Chunk& data);

    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_descr_Info& descr);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_annot_Info& annot);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_annot_place_Info& place);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_data_Info& data);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_assembly_Info& data);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Bioseq_place_Info& data);
    static void x_Attach(CTSE_Chunk_Info& chunk,
                         const CID2S_Seq_feat_Ids_Info& ids);

    typedef CSeq_id_Handle TLocationId;
    typedef CRange<TSeqPos> TLocationRange;
    typedef pair<TLocationId, TLocationRange> TLocation;
    typedef vector<TLocation> TLocationSet;

    static void x_ParseLocation(TLocationSet& vec, const CID2S_Seq_loc& loc);

protected:
    static void x_AddWhole(TLocationSet& vec, const TLocationId& id);
    static void x_AddInterval(TLocationSet& vec, const TLocationId& id,
                              TSeqPos start, TSeqPos length);
    static void x_AddGiWhole(TLocationSet& vec, TGi gi);
    static void x_AddGiInterval(TLocationSet& vec, TGi gi,
                                TSeqPos start, TSeqPos length);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//ID2_PARSER__HPP_INCLUDED
