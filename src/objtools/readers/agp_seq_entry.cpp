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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Convert an AGP file into a vector of Seq-entries
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/agp_seq_entry.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <util/static_map.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

CAgpToSeqEntry::CAgpToSeqEntry(
    TFlags fFlags,
    EAgpVersion agp_version,
    CAgpErr* arg)
    : CAgpReader( arg, agp_version ),
      m_fFlags(fFlags)
{ }

// static
CRef<CSeq_id> CAgpToSeqEntry::s_DefaultSeqIdFromStr( const string & str )
{
    CRef<CSeq_id> seq_id;
    try {
        seq_id.Reset( new CSeq_id( str ) );
    } catch(...) {
        // couldn't create real seq-id.  fall back on local seq-id
        return s_LocalSeqIdFromStr(str);
    }
    return seq_id;
}

// static
CRef<objects::CSeq_id>
    CAgpToSeqEntry::s_LocalSeqIdFromStr( const std::string & str )
{
    CTempString sLocalID(str);

    // (trim off the lcl|, if any)
    CTempString sPrefixToRemove("lcl|");
    if( NStr::StartsWith(sLocalID, sPrefixToRemove, NStr::eNocase) ) {
        sLocalID = sLocalID.substr(sPrefixToRemove.length());
    }

    CRef<CSeq_id> seq_id( new CSeq_id );

    // check if it's a number or string
    const int id_as_num = NStr::StringToInt(sLocalID,
        NStr::fConvErr_NoThrow |
        NStr::fAllowLeadingSpaces |
        NStr::fAllowTrailingSpaces );
    if( id_as_num > 0 ) {
        seq_id->SetLocal().SetId( id_as_num );
    } else {
        seq_id->SetLocal().SetStr( sLocalID );
    }
    return seq_id;
}

void CAgpToSeqEntry::OnGapOrComponent(void)
{
    if( ! m_bioseq ||
        m_prev_row->GetObject() != m_this_row->GetObject() )
    {
        x_FinishedBioseq();

        // initialize new bioseq
        CRef<CSeq_inst> seq_inst( new CSeq_inst );
        seq_inst->SetRepr(CSeq_inst::eRepr_delta);
        seq_inst->SetMol(CSeq_inst::eMol_dna);
        seq_inst->SetLength(0);

        m_bioseq.Reset( new CBioseq );
        m_bioseq->SetInst(*seq_inst);

        m_bioseq->SetId().push_back(
            s_LocalSeqIdFromStr(m_this_row->GetObject()) );
    }

    CRef<CSeq_inst> seq_inst( & m_bioseq->SetInst() );

    CRef<CDelta_seq> delta_seq( new CDelta_seq );
    seq_inst->SetExt().SetDelta().Set().push_back(delta_seq);

    if( m_this_row->is_gap ) {
        delta_seq->SetLiteral().SetLength(m_this_row->gap_length);
        if( m_this_row->component_type == 'U' ) {
            delta_seq->SetLiteral().SetFuzz().SetLim();
        }
        if( m_fFlags & fSetSeqGap ) {
            CSeq_data::TGap & gap_info =
                delta_seq->SetLiteral().SetSeq_data().SetGap();
            x_SetSeqGap(gap_info);
        }
        seq_inst->SetLength() += m_this_row->gap_length;
    } else {
        CSeq_loc& loc = delta_seq->SetLoc();

        CRef<CSeq_id> comp_id =
            x_GetSeqIdFromStr( m_this_row->GetComponentId() );
        loc.SetInt().SetId(*comp_id);

        loc.SetInt().SetFrom( m_this_row->component_beg - 1 );
        loc.SetInt().SetTo(   m_this_row->component_end - 1 );
        seq_inst->SetLength() += ( m_this_row->component_end - m_this_row->component_beg + 1 );

        switch( m_this_row->orientation ) {
        case CAgpRow::eOrientationPlus:
            loc.SetInt().SetStrand( eNa_strand_plus );
            break;
        case CAgpRow::eOrientationMinus:
            loc.SetInt().SetStrand( eNa_strand_minus );
            break;
        case CAgpRow::eOrientationUnknown:
            loc.SetInt().SetStrand( eNa_strand_unknown );
            break;
        case CAgpRow::eOrientationIrrelevant:
            loc.SetInt().SetStrand( eNa_strand_other );
            break;
        default:
            throw runtime_error("unknown orientation " + NStr::IntToString(m_this_row->orientation));
        }
    }
}

int CAgpToSeqEntry::Finalize(void)
{
    // First, do real finalize
    const int return_val = CAgpReader::Finalize();
    // Then, our own finalization
    x_FinishedBioseq();

    return return_val;
}

void CAgpToSeqEntry::x_FinishedBioseq(void)
{
    if( m_bioseq ) {
        CRef<CSeq_entry> entry( new CSeq_entry );
        entry->SetSeq(*m_bioseq);
        m_entries.push_back( entry );

        m_bioseq.Reset();
    }
}

CRef<CSeq_id>
CAgpToSeqEntry::x_GetSeqIdFromStr( const std::string & str )
{
    if( m_fFlags & fForceLocalId ) {
        return s_LocalSeqIdFromStr(str);
    } else {
        return s_DefaultSeqIdFromStr(str);
    }
}

void CAgpToSeqEntry::x_SetSeqGap( CSeq_gap & out_gap_info )
{
    // convert the CAgpRow types to NCBI Objects types

    // The parent class should have verified that the gap-type,
    // linkage, linkage-evidence combos are all consistent

    // gap type
    {{
        // conversion table
        typedef SStaticPair<CAgpRow::EGap, CSeq_gap::EType> TGapTrans;
        static const TGapTrans sc_GapTrans[] = {
            { CAgpRow::eGapClone, CSeq_gap::eType_clone },
            { CAgpRow::eGapFragment, CSeq_gap::eType_fragment },
            { CAgpRow::eGapRepeat, CSeq_gap::eType_repeat },
            { CAgpRow::eGapScaffold, CSeq_gap::eType_scaffold },
            { CAgpRow::eGapContig, CSeq_gap::eType_contig },
            { CAgpRow::eGapCentromere, CSeq_gap::eType_centromere },
            { CAgpRow::eGapShort_arm, CSeq_gap::eType_short_arm },
            { CAgpRow::eGapHeterochromatin, CSeq_gap::eType_heterochromatin },
            { CAgpRow::eGapTelomere, CSeq_gap::eType_telomere }
        };
        typedef CStaticPairArrayMap<CAgpRow::EGap, CSeq_gap::EType> TGapMap;
        DEFINE_STATIC_ARRAY_MAP(TGapMap, sc_GapMap, sc_GapTrans);

        TGapMap::const_iterator find_iter =
            sc_GapMap.find(m_this_row->gap_type);

        if( find_iter == sc_GapMap.end() ) {
            NCBI_USER_THROW_FMT("invalid gap type: "
                << static_cast<int>(m_this_row->gap_type) );
        } else {
            out_gap_info.SetType( find_iter->second );
        }
    }}

    // gap linkage
    {{
        out_gap_info.SetLinkage( m_this_row->linkage ?
            CSeq_gap::eLinkage_linked :
            CSeq_gap::eLinkage_unlinked );
    }}

    // gap linkage-evidence
    {{
        if( m_this_row->linkage_evidence_flags > 0 )
        {
            // conversion table
            typedef SStaticPair<CAgpRow::ELinkageEvidence, CLinkage_evidence::EType> TEvidTrans;
            static const TEvidTrans sc_EvidTrans[] = {
                { CAgpRow::fLinkageEvidence_paired_ends, CLinkage_evidence::eType_paired_ends },
                { CAgpRow::fLinkageEvidence_align_genus, CLinkage_evidence::eType_align_genus },
                { CAgpRow::fLinkageEvidence_align_xgenus, CLinkage_evidence::eType_align_xgenus },
                { CAgpRow::fLinkageEvidence_align_trnscpt, CLinkage_evidence::eType_align_trnscpt },
                { CAgpRow::fLinkageEvidence_within_clone, CLinkage_evidence::eType_within_clone },
                { CAgpRow::fLinkageEvidence_clone_contig, CLinkage_evidence::eType_clone_contig },
                { CAgpRow::fLinkageEvidence_map, CLinkage_evidence::eType_map },
                { CAgpRow::fLinkageEvidence_strobe, CLinkage_evidence::eType_strobe },
                { CAgpRow::fLinkageEvidence_pcr, CLinkage_evidence::eType_pcr },
                { CAgpRow::fLinkageEvidence_proximity_ligation, CLinkage_evidence::eType_proximity_ligation }
            };
            typedef CStaticPairArrayMap<CAgpRow::ELinkageEvidence, CLinkage_evidence::EType> TEvidMap;
            DEFINE_STATIC_ARRAY_MAP(TEvidMap, sc_EvidMap, sc_EvidTrans);

            CSeq_gap::TLinkage_evidence & link_evid =
                out_gap_info.SetLinkage_evidence();

            _ASSERT( ! m_this_row->linkage_evidences.empty() );
            ITERATE( CAgpRow::TLinkageEvidenceVec, evid_it,
                m_this_row->linkage_evidences )
            {
                const CAgpRow::ELinkageEvidence eLinkageEvidence =
                    *evid_it;
                TEvidMap::const_iterator find_iter =
                    sc_EvidMap.find(eLinkageEvidence);
                if( find_iter == sc_EvidMap.end() ) {
                    NCBI_USER_THROW_FMT("Unknown linkage evidence: "
                        << static_cast<int>(eLinkageEvidence) );
                }

                CRef<CLinkage_evidence> pEvid( new CLinkage_evidence );
                pEvid->SetType( find_iter->second );
                link_evid.push_back( pEvid );
            }
        } else {
            // check special values
            _ASSERT( m_this_row->linkage_evidences.empty() );
            switch( m_this_row->linkage_evidence_flags ) {
            case CAgpRow::fLinkageEvidence_unspecified:
                {
                    CRef<CLinkage_evidence> pEvid( new CLinkage_evidence );
                    pEvid->SetType( CLinkage_evidence::eType_unspecified );
                    out_gap_info.SetLinkage_evidence().push_back( pEvid );
                }
                break;
            case CAgpRow::fLinkageEvidence_na:
                // no problem, just ignore
                break;
            default:
                NCBI_USER_THROW_FMT(
                    "Unknown or unexpected linkage_evidence_flags: "
                    << m_this_row->linkage_evidence_flags);
                break;
            }
        }
    }}
}

END_NCBI_SCOPE
