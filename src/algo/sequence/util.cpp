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
 * Authors:  Christiam Camacho
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/util.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/seqport_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRef<objects::CBioseq>
SeqLocToBioseq(const objects::CSeq_loc& loc, objects::CScope& scope)
{
    CRef<CBioseq> bioseq;
    if ( !loc.GetId() ) {
        return bioseq;
    }

    // Build a Seq-entry for the query Seq-loc
    CBioseq_Handle handle = scope.GetBioseqHandle(*loc.GetId());
    if( !handle ){
        return bioseq;
    }

    bioseq.Reset( new CBioseq() );

    // add an ID for our sequence
    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*handle.GetSeqId());
    bioseq->SetId().push_back(id);

    // a title
    CRef<CSeqdesc> title( new CSeqdesc() );
    string title_str;
    id -> GetLabel(&title_str );
    title_str += ": ";
    loc.GetLabel( &title_str );
    title->SetTitle( title_str );
    bioseq->SetDescr().Set().push_back( title );

    ///
    /// create the seq-inst
    /// we can play some games here
    ///
    CSeq_inst& inst = bioseq->SetInst();

    if (handle.IsAa()) {
        inst.SetMol(CSeq_inst::eMol_aa);
    } else {
        inst.SetMol(CSeq_inst::eMol_na);
    }

    bool process_whole = false;
    if (loc.IsWhole()) {
        process_whole = true;
    } else if (loc.IsInt()) {
        TSeqRange range = loc.GetTotalRange();
        if (range.GetFrom() == 0  &&  range.GetTo() == handle.GetBioseqLength() - 1) {
            /// it's whole
            process_whole = true;
        }
    }

    /// BLAST now handles delta-seqs correctly, so we can submit this
    /// as a delta-seq
    if (process_whole) {
        /// just encode the whole sequence
        CSeqVector vec(loc, scope, CBioseq_Handle::eCoding_Iupac);
        string seq_string;
        vec.GetSeqData(0, vec.size(), seq_string);

        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetLength(seq_string.size());
        if (vec.IsProtein()) {
            inst.SetMol(CSeq_inst::eMol_aa);
            inst.SetSeq_data().SetIupacaa().Set().swap(seq_string);
        } else {
            inst.SetMol(CSeq_inst::eMol_na);
            inst.SetSeq_data().SetIupacna().Set().swap(seq_string);
            CSeqportUtil::Pack(&inst.SetSeq_data());
        }
    } else {
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.SetLength(handle.GetBioseqLength());
        CDelta_ext& ext = inst.SetExt().SetDelta();

        ///
        /// create a delta sequence
        ///

        //const CSeq_id& id = sequence::GetId(loc, &scope);
        //ENa_strand strand = sequence::GetStrand(loc, &scope);
        TSeqRange range = loc.GetTotalRange();

        /// first segment: gap out to initial start of seq-loc
        if (range.GetFrom() != 0) {
            ext.AddLiteral(range.GetFrom());
        }

        CSeq_loc_CI loc_iter(loc);
        if (loc_iter) {
            TSeqRange  prev   = loc_iter.GetRange();
            ENa_strand strand = loc_iter.GetStrand();

            do {
                /// encode a literal for the included bases
                CRef<CSeq_loc> sl =
                    handle.GetRangeSeq_loc(prev.GetFrom(), prev.GetTo(), strand);

                CSeqVector vec(*sl, scope, CBioseq_Handle::eCoding_Iupac);
                string seq_string;
                vec.GetSeqData(0, vec.size(), seq_string);

                ext.AddLiteral(seq_string,
                    (vec.IsProtein() ? CSeq_inst::eMol_aa : CSeq_inst::eMol_na));

                /// skip to the next segment
                /// we may need to include a gap
                ++loc_iter;
                if (loc_iter) {
                    TSeqRange next = loc_iter.GetRange();
                    ext.AddLiteral(next.GetFrom() - prev.GetTo());
                    prev = next;
                    strand = loc_iter.GetStrand();
                }
            }
            while (loc_iter);

            /// gap at the end
            if (prev.GetTo() < handle.GetBioseqLength() - 1) {
                ext.AddLiteral(handle.GetBioseqLength() - prev.GetTo() - 1);
            }
        }
    }

    return bioseq;
}

END_NCBI_SCOPE
