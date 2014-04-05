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
 * Authors:  Josh Cherry
 *
 * File Description:  Write agp file
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/writers/agp_write.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static char s_DetermineComponentType(const CSeq_id& id, CScope& scope);

// This could be made considerably faster (conversion of enum to string)
inline string GetGapType(const CSeqMap_CI& iter,
                         const string* default_gap_type)
{
    try {
        const CSeq_data& data = iter.GetData();
        CSeq_gap::TType type = data.GetGap().GetType();
        switch (type) {
        case CSeq_gap::eType_fragment:
            return "fragment";
        case CSeq_gap::eType_clone:
            return "clone";
        case CSeq_gap::eType_short_arm:
            return "short_arm";
        case CSeq_gap::eType_heterochromatin:
            return "heterochromatin";
        case CSeq_gap::eType_centromere:
            return "centromere";
        case CSeq_gap::eType_telomere:
            return "telomere";
        case CSeq_gap::eType_repeat:
            return "repeat";
        case CSeq_gap::eType_contig:
            return "contig";
        case CSeq_gap::eType_scaffold:
            return "scaffold";
        default:
            // unknown or other
            // do nothing (will use default_gap_type if non-null)
            ;
        }
    } catch (CSeqMapException&) {
        // no Seq-data for this gap
    }
    if (default_gap_type) {
        return *default_gap_type;
    }
    throw runtime_error("couldn't get gap type");
}

inline bool GetLinkage(const CSeqMap_CI& iter,
                       const bool* default_linkage)
{
    try {
        return iter.GetData().GetGap().GetLinkage()
            == CSeq_gap::eLinkage_linked;
    } catch (CSeqMapException&) {
        // no Seq-data for this gap
    }
    if (default_linkage) {
        return *default_linkage;
    }
    throw runtime_error("couldn't get linkage");
}

static
inline void WriteLinkageEvidence( CNcbiOstream &os, const CSeqMap_CI& iter )
{
    try {
        const CSeq_gap & gap = iter.GetData().GetGap();
        if( gap.IsSetLinkage_evidence() ) {
            string linkage_evidence_str;
            CLinkage_evidence::VecToString( 
                linkage_evidence_str, 
                gap.GetLinkage_evidence() );
            os << linkage_evidence_str;
        }
    } catch (CSeqMapException&) {
        // no Seq-data for this gap
    }
}


/// Write to stream in agp format.
/// Based on www.ncbi.nlm.nih.gov/genome/guide/Assembly/AGP_Specification.html
static void s_AgpWrite(CNcbiOstream& os,
                       const CSeqMap& seq_map,
                       TSeqPos start_pos,
                       TSeqPos stop_pos,
                       const string& object_id,
                       const string* default_gap_type,
                       const bool* default_linkage,
                       CScope& scope,
                       const vector<char>& component_types,
                       CAgpWriteComponentIdMapper * comp_id_mapper,
                       int agp_version)
{
    unsigned int count = 0;

    if (!component_types.empty()
        && component_types.size() != seq_map.GetSegmentsCount()) {
        string error_str = "length of component_types ("
            + NStr::NumericToString(component_types.size())
            + ") is inconsistent with number of segments ("
            + NStr::NumericToString(seq_map.GetSegmentsCount()) + ")";
        NCBI_THROW(CObjWriterException, eArgErr, error_str);
    }

    for (CSeqMap_CI iter(seq_map.Begin(&scope));  iter;  ++iter, ++count) {
        TSeqPos seg_pos = iter.GetPosition();
        TSeqPos seg_end = iter.GetPosition() + iter.GetLength();
        if (start_pos > seg_end) {
            continue;
        }

        TSeqPos start_offs = 0;
        if (start_pos > seg_pos) {
            start_offs = start_pos - seg_pos;
        }

        if (stop_pos < seg_pos) {
            /// done!
            break;
        }

        TSeqPos end_offs = 0;
        if (seg_end > stop_pos) {
            end_offs = seg_end - stop_pos;
        }

        // col 1: the object ID
        os << object_id;

        // col 2: start position on this object
        os << '\t' << seg_pos + start_offs + 1;

        // col 3: end position on this object
        os << '\t' << seg_end - end_offs;

        // col 4: part number
        os << '\t' << count + 1;  // 1-based

        switch (iter.GetType()) {
        case CSeqMap::eSeqGap:
            // col 5
            os << '\t';
            if (component_types.empty()) {
                os << 'N';
            } else {
                os << component_types[count];
            }
            // col 6b
            os << '\t' << iter.GetLength();
            // col 7b
            os << '\t' << GetGapType(iter, default_gap_type);
            // col 8b
            os << '\t' << (GetLinkage(iter, default_linkage) ? "yes" : "no");
            // col 9; Write an empty column.  The spec says there should
            //        be an empty column, rather than no column (i.e., no tab).
            os << '\t';
            if(agp_version >= 2) {
                if(!GetLinkage(iter, default_linkage))
                    os << "na";
                else
                    WriteLinkageEvidence( os, iter );
            }
            break;

        case CSeqMap::eSeqRef:
            // col 5; Should be A, D, F, G, P, O, or W
            os << '\t';
            if (component_types.empty()) {
                os <<
                    s_DetermineComponentType(*iter.GetRefSeqid().GetSeqId(),
                                             scope);
            } else {
                os << component_types[count];
            }
            // col 6a
            os << '\t';
            {{
                CSeq_id_Handle idh =
                    sequence::GetId(*iter.GetRefSeqid().GetSeqId(), scope,
                                    sequence::eGetId_ForceAcc);
                string id_str;
                idh.GetSeqId()->GetLabel(&id_str, CSeq_id::eContent);
                if( comp_id_mapper != NULL ) {
                    comp_id_mapper->do_map( id_str );
                }
                os << id_str;
            }}


            // col 7a
            os << '\t' << iter.GetRefPosition() + start_offs + 1;
            // col 8a
            os << '\t' << iter.GetRefEndPosition() - end_offs;
            // col 9
            if (iter.GetRefMinusStrand()) {
                os << "\t-";
            } else {
                os << "\t+";
            }
            break;

        default:
            break;
        }
        os << '\n';
    }
}


static CConstRef<CSeqMap> s_SeqMapForHandle(const CBioseq_Handle& handle)
{
    CConstRef<CSeqMap> seq_map;
    bool treat_as_raw;           // one line, itself as component

    if (handle.GetInst_Repr() == CSeq_inst::eRepr_raw) {
        treat_as_raw = true;
    } else if (handle.GetInst_Repr() == CSeq_inst::eRepr_delta) {
        // Check for a delta of all non-gap literals
        treat_as_raw = true;
        ITERATE (CDelta_ext::Tdata,
                 iter,
                 handle.GetInst_Ext().GetDelta().Get()) {
            if ((*iter)->IsLoc() || !(*iter)->GetLiteral().IsSetSeq_data()) {
                treat_as_raw = false;
                break;
            }
        }
    } else {
        treat_as_raw = false;
    }

    if (treat_as_raw) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetInt().SetId().Assign(*handle.GetSeq_id_Handle().GetSeqId());
        loc->SetInt().SetFrom(0);
        loc->SetInt().SetTo(handle.GetBioseqLength() - 1);
        seq_map = CSeqMap::CreateSeqMapForSeq_loc(*loc, &handle.GetScope());
    } else {
        seq_map = &handle.GetSeqMap();
    }
    return seq_map;
}


void AgpWrite(CNcbiOstream& os,
              const CSeqMap& seq_map,
              const string& object_id,
              CScope& scope,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, seq_map, 0, seq_map.GetLength(&scope),
               object_id, NULL, NULL, scope, component_types,
               comp_id_mapper, agp_version);
}

void AgpWrite(CNcbiOstream& os,
              const CBioseq_Handle& handle,
              const string& object_id,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, *s_SeqMapForHandle(handle), 0, handle.GetBioseqLength(),
               object_id, NULL, NULL,
               handle.GetScope(), component_types, comp_id_mapper, agp_version);
}


void AgpWrite(CNcbiOstream& os,
              const CBioseq_Handle& handle,
              TSeqPos from, TSeqPos to,
              const string& object_id,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, *s_SeqMapForHandle(handle), from, to,
               object_id, NULL, NULL,
               handle.GetScope(), component_types, comp_id_mapper, agp_version);
}


void AgpWrite(CNcbiOstream& os,
              const CSeqMap& seq_map,
              const string& object_id,
              const string& default_gap_type,
              bool default_linkage,
              CScope& scope,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, seq_map, 0, seq_map.GetLength(&scope),
               object_id, &default_gap_type, &default_linkage,
               scope, component_types, comp_id_mapper, agp_version);
}

void AgpWrite(CNcbiOstream& os,
              const CBioseq_Handle& handle,
              const string& object_id,
              const string& default_gap_type,
              bool default_linkage,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, *s_SeqMapForHandle(handle), 0, handle.GetBioseqLength(),
               object_id, &default_gap_type, &default_linkage,
               handle.GetScope(), component_types, comp_id_mapper, agp_version);
}


void AgpWrite(CNcbiOstream& os,
              const CBioseq_Handle& handle,
              TSeqPos from, TSeqPos to,
              const string& object_id,
              const string& default_gap_type,
              bool default_linkage,
              const vector<char>& component_types,
              CAgpWriteComponentIdMapper * comp_id_mapper,
              int agp_version )
{
    s_AgpWrite(os, *s_SeqMapForHandle(handle), from, to,
               object_id, &default_gap_type, &default_linkage,
               handle.GetScope(), component_types, comp_id_mapper, agp_version);
}


// This function tries to figure out the "component type"
// to put into column 5 (F, A, D, P, W, or O) by examining
// the referenced Bioseq.
// The relevant attributes are htgs-[0123] and wgs in MolInfo.tech
// and the GB-block keywords HTGS_DRAFT, HTGS_FULLTOP, and HTGS_ACTIVEFIN.
// The rules are as follows. wgs trumps everything, yieding W.
// htgs-[012] indicate P (pre-draft), htgs-3 indicates F (finished),
// HTGS_DRAFT and HTGS_FULLTOP indicate D (draft), and HTGS_ACTIVEFIN
// indicates A (active finishing).  If more than one of F, A, D, P
// are indicated, the "more finished" takes precedence, i.e.,
// F > A > D > P, and the maximum according to this ordering is used.
// If none of these attributes are present, an O (other) is returned.

enum EAgpType
{
    eO,
    eP,
    eD,
    eA,
    eF
};

static char s_DetermineComponentType(const CSeq_id& id, CScope& scope)
{
    EAgpType type = eO;

    CSeqdesc_CI::TDescChoices ch;
    ch.push_back(CSeqdesc::e_Molinfo);
    ch.push_back(CSeqdesc::e_Genbank);

    CSeqdesc_CI desc_iter(scope.GetBioseqHandle(id), ch);
    for ( ;  desc_iter;  ++desc_iter) {
        switch (desc_iter->Which()) {
        case CSeqdesc::e_Molinfo:
            switch (desc_iter->GetMolinfo().GetTech()) {
            case CMolInfo::eTech_wgs:
                return 'W';
            case CMolInfo::eTech_htgs_0:
            case CMolInfo::eTech_htgs_1:
            case CMolInfo::eTech_htgs_2:
                type = max(type, eP);
                break;
            case CMolInfo::eTech_htgs_3:
                type = max(type, eF);
                break;
            default:
                break;
            }
            break;

        case CSeqdesc::e_Genbank:
            if (desc_iter->GetGenbank().CanGetKeywords()) {
                ITERATE (CGB_block::TKeywords, kw,
                         desc_iter->GetGenbank().GetKeywords()) {
                    if (*kw == "HTGS_DRAFT" || *kw == "HTGS_FULLTOP") {
                        type = max(type, eD);
                    }
                    if (*kw == "HTGS_ACTIVEFIN") {
                        type = max(type, eA);
                    }
                }
            }
            break;

        default:
            _ASSERT(false);
            break;
        }
    }

    switch (type) {
    case eP:
        return 'P';
    case eD:
        return 'D';
    case eA:
        return 'A';
    case eF:
        return 'F';
    default:
        return 'O';
    }
}

END_NCBI_SCOPE
