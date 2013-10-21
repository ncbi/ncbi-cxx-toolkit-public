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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <algo/sequence/internal_stops.hpp>
#include "feature_generator.hpp"

#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

CInternalStopFinder::CInternalStopFinder(CScope& a_scope)
    : scope(a_scope), generator(a_scope)
{
    generator.SetFlags(CFeatureGenerator::fTrimEnds);
    generator.SetAllowedUnaligned(10);
}

string CInternalStopFinder::GetProtein(const CSeq_align& align)
{
    const string seq = GetCDSNucleotideSequence(align);

    CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(1));

    int gcode = 11;
    try {
        gcode = sequence::GetOrg_ref(bsh).GetGcode();
    } catch (CException&) {
    }

    CRef<CGenetic_code::C_E> c_e(new CGenetic_code::C_E); c_e->SetId(gcode);
    CRef<CGenetic_code> code(new CGenetic_code);
    code->Set().push_back(c_e);

    string prot;
    CSeqTranslator::Translate(seq,
              prot,
              CSeqTranslator::fIs5PrimePartial,
              code.GetPointer());
    return prot;
}

set<TSeqPos> CInternalStopFinder::FindStops(const CSeq_align& align)
{
    CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align);
    string prot = GetProtein(*clean_align);

    CSeq_loc_Mapper mapper(*clean_align, 1);

    CRef<CSeq_loc> query_loc(new CSeq_loc);
    const CSpliced_seg& spl = align.GetSegs().GetSpliced();
    query_loc->SetInt(*spl.GetExons().front()->CreateRowSeq_interval(0, spl));

    set<TSeqPos> stops;

    int pos = -1;
    while (size_t(pos = prot.find('*', pos+1)) != NPOS) {
        query_loc->SetInt().SetFrom(pos);
        query_loc->SetInt().SetTo(pos);

        CRef<CSeq_loc> stop_codon = mapper.Map(*query_loc);

        stops.insert(stop_codon->GetStart(eExtreme_Biological));
    }

    return stops;
}

bool CInternalStopFinder::HasInternalStops(const CSeq_align& align)
{
    CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align);
    string prot = GetProtein(*clean_align);
    return  prot.find('*') != NPOS;
}

string CInternalStopFinder::GetCDSNucleotideSequence(const CSeq_align& align)
{
    string mRNA;

    const CSpliced_seg& spliced_seg = align.GetSegs().GetSpliced();

    int next_prod_start = 0;
    int cds_len = spliced_seg.GetProduct_length();

    if (spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_transcript) {
        const CSeq_id& product_id = spliced_seg.GetProduct_id();
        CMappedFeat cds_on_rna = GetCdsOnMrna(product_id, scope);
        if (cds_on_rna.GetLocation().GetStrand() == eNa_strand_minus) {
            NCBI_THROW(CException, eUnknown, "minus strand cdregion on mrna is not supported");
        }
        next_prod_start = cds_on_rna.GetLocation().GetStart(eExtreme_Biological);
        cds_len = cds_on_rna.GetLocation().GetTotalRange().GetLength();

        if (!cds_on_rna.GetLocation().IsPartialStop(eExtreme_Biological)) {
            cds_len -= 3;
        }
    } else {
        cds_len *=3;
    }

    ITERATE( CSpliced_seg::TExons, exon, spliced_seg.GetExons() ) {

        int prod_pos_start = (*exon)->GetProduct_start().AsSeqPos();
        int prod_pos_end = (*exon)->GetProduct_end().AsSeqPos();


        CRef<CSeq_loc> subject_loc(new CSeq_loc);
        subject_loc->SetInt(*(*exon)->CreateRowSeq_interval(1, spliced_seg));
        CSeqVector subject_vec(*subject_loc, scope,
                               CBioseq_Handle::eCoding_Iupac);
        string subject_seq;
        subject_vec.GetSeqData(subject_vec.begin(), subject_vec.end(), subject_seq);
        int subj_pos = 0;

        if (next_prod_start < prod_pos_start) {
            mRNA.append(prod_pos_start - next_prod_start, 'N');
            next_prod_start = prod_pos_start;
        }

        if ((*exon)->IsSetParts()) {
            ITERATE (CSpliced_exon::TParts, part_it, (*exon)->GetParts()) {
                pair<int, int> chunk = ChunkSize(**part_it);
                prod_pos_start += chunk.second;
                if (chunk.first == 0) {
                    if (next_prod_start < prod_pos_start) {
                        mRNA.append(prod_pos_start - next_prod_start, 'N');
                        next_prod_start = prod_pos_start;
                    }
                } else if (chunk.second > 0) {
                    if (next_prod_start < prod_pos_start) {
                        mRNA.append(subject_seq, subj_pos+chunk.second-(prod_pos_start - next_prod_start), prod_pos_start - next_prod_start);
                        next_prod_start = prod_pos_start;
                    }
                }
                subj_pos += chunk.first;
            }
        } else {
            mRNA.append(subject_seq);
        }

        _ASSERT( next_prod_start >= prod_pos_end + 1);
    }

    return mRNA.substr(0, cds_len);
}


// pair(genomic, product)
pair<int, int> ChunkSize(const CSpliced_exon_chunk& chunk)
{
    int len = 0;
    switch (chunk.Which()) {
    case CSpliced_exon_chunk::e_Genomic_ins:
        len = chunk.GetGenomic_ins();
        return make_pair(len, 0);
    case CSpliced_exon_chunk::e_Product_ins:
        len = chunk.GetProduct_ins();
        return make_pair(0, len);
    case CSpliced_exon_chunk::e_Match:
        len = chunk.GetMatch();
        break;
    case CSpliced_exon_chunk::e_Mismatch:
        len = chunk.GetMismatch();
        break;
    case CSpliced_exon_chunk::e_Diag:
        len = chunk.GetDiag();
        break;
    default:
        NCBI_THROW(CException, eUnknown, "Spliced_exon_chunk type not set");
    }
    return make_pair(len, len);
}

END_NCBI_SCOPE
