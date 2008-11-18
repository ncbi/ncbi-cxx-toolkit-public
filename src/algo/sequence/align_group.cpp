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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/align_group.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objtools/alnmgr/alnmix.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CAlignGroup::CAlignGroup()
{
}


CAlignGroup::~CAlignGroup()
{
}


void CAlignGroup::GroupByTaxIds(const TAlignList& aligns,
                                TAnnotList&       align_groups,
                                const string&     annot_name_base,
                                CScope&           scope)
{
    TTaxAlignMap tax_aligns;
    x_SeparateByTaxId(aligns, tax_aligns, scope);

    ///
    /// now, package as separate annots
    ///
    NON_CONST_ITERATE (TTaxAlignMap, iter, tax_aligns) {
        string tax_id_tag;
        ITERATE (set<int>, it, iter->first) {
            CConstRef<COrg_ref> org_ref = x_GetOrgRef(*it);
            if ( !tax_id_tag.empty() ) {
                tax_id_tag += "; ";
            }
            if (org_ref) {
                org_ref->GetLabel(&tax_id_tag);
            } else {
                tax_id_tag += "unknown";
            }

            tax_id_tag += " [taxid:" + NStr::IntToString(*it) + "]";
        }

        CRef<CSeq_annot> annot(new CSeq_annot);

        string name(annot_name_base);
        if ( !name.empty() ) {
            name += ": ";
        }
        name += tax_id_tag;
        annot->SetName(name);
        annot->SetData().SetAlign().swap(iter->second);
        align_groups.push_back(annot);
    }
}


void CAlignGroup::GroupByLikeTaxIds(const TAlignList& aligns,
                                    TAnnotList&       align_groups,
                                    const string&     annot_name_base,
                                    CScope&           scope)
{
    TTaxAlignMap tax_aligns;
    x_SeparateByTaxId(aligns, tax_aligns, scope);

    ///
    /// now, package as separate annots
    ///
    CRef<CSeq_annot> mixed_annot;
    NON_CONST_ITERATE (TTaxAlignMap, iter, tax_aligns) {
        if (iter->first.size() == 1) {
            string tax_id_tag;
            int tax_id = *iter->first.begin();
            CConstRef<COrg_ref> org_ref = x_GetOrgRef(tax_id);

            if ( !tax_id_tag.empty() ) {
                tax_id_tag += "; ";
            }
            if (org_ref) {
                org_ref->GetLabel(&tax_id_tag);
            } else {
                tax_id_tag += "unknown";
            }

            tax_id_tag += " [taxid:" + NStr::IntToString(tax_id) + "]";

            CRef<CSeq_annot> annot(new CSeq_annot);

            string name(annot_name_base);
            if ( !name.empty() ) {
                name += ": ";
            }
            name += tax_id_tag;
            annot->SetName(name);
            annot->SetData().SetAlign().swap(iter->second);
            align_groups.push_back(annot);
        } else {
            if ( !mixed_annot ) {
                mixed_annot.Reset(new CSeq_annot);
                string name(annot_name_base);
                if ( !name.empty() ) {
                    name += ": ";
                }
                name += "Mixed Taxa";
                mixed_annot->SetName(name);
            }
            mixed_annot->SetData().SetAlign()
                .insert(mixed_annot->SetData().SetAlign().end(),
                        iter->second.begin(), iter->second.end());
        }
    }

    if (mixed_annot) {
        align_groups.push_back(mixed_annot);
    }
}


void CAlignGroup::GroupBySeqIds(const TAlignList& alignments,
                                TAnnotList&       align_groups,
                                const string&     annot_name_base,
                                objects::CScope&  scope,
                                TSeqIdFlags       flags)
{
    /// typedefs for dealing with separations by sequence types
    typedef set<CSeq_id_Handle> TSeqIds;
    typedef map<TSeqIds, list< CRef<objects::CSeq_align> > > TSequenceAlignMap;

    ///
    /// first, categorize these types
    ///
    TSequenceAlignMap seq_aligns;
    ITERATE (TAlignList, iter, alignments) {
        CRef<CSeq_align> align = *iter;

        TSeqIds ids;
        CTypeConstIterator<CSeq_id> id_iter(*align);
        for ( ;  id_iter;  ++id_iter) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id_iter);
            if (flags & fResolveToGi) {
                CSeq_id_Handle gi_idh =
                    sequence::GetId(idh, scope, sequence::eGetId_ForceGi);
                if (gi_idh) {
                    idh = gi_idh;
                }
            }
            ids.insert(idh);
        }

        seq_aligns[ids].push_back(align);
    }

    ///
    /// now, create annotations for these as needed
    /// order here is important, as the binning is destructive
    ///

    ITERATE (TSequenceAlignMap, iter, seq_aligns) {
        if ( !iter->second.size() ) {
            continue;
        }

        string tag;
        /// scan to see if the IDs contains something appropriate
        ITERATE (TSeqIds, it, iter->first) {
            if ( !tag.empty() ) {
                tag += "x";
            }
            it->GetSeqId()->GetLabel(&tag,
                CSeq_id::eContent,
                CSeq_id::fLabel_Version | CSeq_id::fLabel_GeneralDbIsContent);
        }

        CRef<CSeq_annot> annot(new CSeq_annot);
        annot->SetData().SetAlign()
            .insert(annot->SetData().SetAlign().begin(),
                    iter->second.begin(), iter->second.end());

        string name(annot_name_base);
        if ( !name.empty() ) {
            name += ": ";
        }
        name += tag;
        annot->SetName(name);
        align_groups.push_back(annot);
    }
}


void CAlignGroup::GroupByStrand(const TAlignList& alignments,
                                TAnnotList&       align_groups,
                                const string&     annot_name_base,
                                objects::CScope&  scope)
{
    /// typedefs for dealing with separations by sequence types
    typedef set<ENa_strand> TStrands;
    typedef map<TStrands, list< CRef<objects::CSeq_align> > > TSequenceAlignMap;

    ///
    /// first, categorize these types
    ///
    TSequenceAlignMap seq_aligns;
    ITERATE (TAlignList, iter, alignments) {
        CRef<CSeq_align> align = *iter;

        TStrands strands;
        CSeq_align::TDim rows = align->CheckNumRows();
        for (CSeq_align::TDim i = 0;  i < rows;  ++i) {
            strands.insert(align->GetSeqStrand(i));
        }
        seq_aligns[strands].push_back(align);
    }

    ///
    /// now, create annotations for these as needed
    /// order here is important, as the binning is destructive
    ///

    ITERATE (TSequenceAlignMap, iter, seq_aligns) {
        if ( !iter->second.size() ) {
            continue;
        }

        string tag;
        /// scan to see if the IDs contains something appropriate
        ITERATE (TStrands, it, iter->first) {
            if ( !tag.empty() ) {
                tag += "/";
            }
            switch (*it) {
            case eNa_strand_minus:
                tag += "-";
                break;
            default:
                tag += "+";
                break;
            }
        }

        CRef<CSeq_annot> annot(new CSeq_annot);
        annot->SetData().SetAlign()
            .insert(annot->SetData().SetAlign().begin(),
                    iter->second.begin(), iter->second.end());

        string name(annot_name_base);
        if ( !name.empty() ) {
            name += ": ";
        }
        name += tag;
        annot->SetName(name);
        align_groups.push_back(annot);
    }
}


void CAlignGroup::GroupBySequenceType(const TAlignList& alignments,
                                      TAnnotList&       align_groups,
                                      const string&     annot_name_base,
                                      objects::CScope&  scope,
                                      TSequenceFlags    flags)
{
    /// typedefs for dealing with separations by sequence types
    typedef set<int> TSequenceTypes;
    typedef map<TSequenceTypes, list< CRef<objects::CSeq_align> > > TSequenceAlignMap;

    ///
    /// first, categorize these types
    ///
    TSequenceAlignMap seq_aligns;
    ITERATE (TAlignList, iter, alignments) {
        CRef<CSeq_align> align = *iter;

        TSequenceTypes types;
        CTypeConstIterator<CSeq_id> id_iter(*align);
        for ( ;  id_iter;  ++id_iter) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id_iter);
            idh = sequence::GetId(idh, scope, sequence::eGetId_Best);

            string id_str;
            idh.GetSeqId()->GetLabel(&id_str);
            CSeq_id::EAccessionInfo info = idh.GetSeqId()->IdentifyAccession();
            TSequenceFlags this_flags = 0;

            /// EST alignments: in EST division
            if (flags & fEST  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) {
                    types.insert(fEST);
                    this_flags |= fEST;
                }
            }

            /// WGS alignments: in WGS division
            if (flags & fWGS  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs) {
                    types.insert(fWGS);
                    this_flags |= fWGS;
                }
            }

            /// HTGS alignments: in HTGS division
            if (flags & fHTGS  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_htgs) {
                    types.insert(fHTGS);
                    this_flags |= fHTGS;
                }
            }

            /// Patent alignments: in Patent division
            if (flags & fPatent  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_div_patent) {
                    types.insert(fPatent);
                    this_flags |= fPatent;
                }
            }

            /// RefSeq predicted alignments:
            /// accession type = other and predicted flag set
            /// this must precede regular refseq!
            if (flags & fRefSeqPredicted  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_type_mask) == CSeq_id::e_Other  &&
                    (info & CSeq_id::fAcc_predicted) == CSeq_id::fAcc_predicted) {
                    types.insert(fRefSeqPredicted);
                    this_flags |= fRefSeqPredicted;
                }
            }

            /// RefSeq predicted alignments:
            /// accession type = other
            if (flags & fRefSeq  &&  !this_flags ) {
                if ((info & CSeq_id::eAcc_type_mask) == CSeq_id::e_Other) {
                    types.insert(fRefSeq);
                    this_flags |= fRefSeq;
                }
            }

            /// RefSeq predicted alignments:
            /// accession type = GenBank, EMBL, or DDBJ
            if (flags & fGB_EMBL_DDBJ  &&  !this_flags ) {
                bool is_gb = false;
                is_gb |= (info & CSeq_id::eAcc_type_mask) == CSeq_id::e_Genbank;
                is_gb |= (info & CSeq_id::eAcc_type_mask) == CSeq_id::e_Embl;
                is_gb |= (info & CSeq_id::eAcc_type_mask) == CSeq_id::e_Ddbj;
                if (is_gb) {
                    types.insert(fGB_EMBL_DDBJ);
                    this_flags |= fGB_EMBL_DDBJ;
                }
            }

            if (this_flags) {
                types.insert(this_flags);
            }
        }

        seq_aligns[types].push_back(align);
    }

    ///
    /// now, create annotations for these as needed
    /// order here is important, as the binning is destructive
    ///

    ITERATE (TSequenceAlignMap, iter, seq_aligns) {
        if ( !iter->second.size() ) {
            continue;
        }

        string tag;
        /// scan to see if the IDs contains something appropriate
        ITERATE (TSequenceTypes, it, iter->first) {
            if ( !tag.empty() ) {
                tag += "/";
            }
            switch (*it) {
            case fEST:
                tag += "EST";
                break;
            case fWGS:
                tag += "WGS";
                break;
            case fHTGS:
                tag += "HTGS";
                break;
            case fPatent:
                tag += "Patent";
                break;
            case fRefSeq:
                tag += "RefSeq";
                break;
            case fRefSeqPredicted:
                tag += "Predicted RefSeq";
                break;
            case fGB_EMBL_DDBJ:
                tag += "GenBank-EMBL-DDBJ";
                break;

            default:
                tag += "Other";
                break;
            }
        }

        CRef<CSeq_annot> annot(new CSeq_annot);
        annot->SetData().SetAlign()
            .insert(annot->SetData().SetAlign().begin(),
                    iter->second.begin(), iter->second.end());

        string name(annot_name_base);
        if ( !name.empty() ) {
            name += ": ";
        }
        name += tag;
        annot->SetName(name);
        align_groups.push_back(annot);
    }
}


void CAlignGroup::x_SeparateByTaxId(const TAlignList& alignments,
                                    TTaxAlignMap&     tax_aligns,
                                    CScope&           scope)
{
    ITERATE (TAlignList, iter, alignments) {
        CRef<CSeq_align> align = *iter;

        TTaxIds ids;
        CTypeConstIterator<CSeq_id> id_iter(*align);
        for ( ;  id_iter;  ++id_iter) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id_iter);
            int tax_id = x_GetTaxId(idh, scope);
            ids.insert(tax_id);
        }

        tax_aligns[ids].push_back(align);
    }
}


int CAlignGroup::x_GetTaxId(const CSeq_id_Handle& id, CScope& scope)
{
    int tax_id = 0;
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandle(id);
        tax_id = sequence::GetTaxId(bsh);
        if ( !tax_id ) {
            if ( !m_Taxon1.get() ) {
                m_Taxon1.reset(new CTaxon1);
                m_Taxon1->Init();
            }
            CSeq_id_Handle gi_idh =
                sequence::GetId(id, scope, sequence::eGetId_ForceGi);
            m_Taxon1->GetTaxId4GI(gi_idh.GetGi(), tax_id);
        }
        m_TaxIds.insert(TTaxIdMap::value_type(id, tax_id));
    }
    catch (CException&) {
    }
    return tax_id;
}


CConstRef<COrg_ref> CAlignGroup::x_GetOrgRef(int tax_id)
{
    CConstRef<COrg_ref> org_ref;
    TTaxInfoMap::iterator tax_iter = m_TaxInfo.find(tax_id);
    if (tax_iter == m_TaxInfo.end()) {
        if (tax_id) {
            if ( !m_Taxon1.get() ) {
                m_Taxon1.reset(new CTaxon1);
                m_Taxon1->Init();
            }
            bool is_species;
            bool is_uncultured;
            string blast_name;
            org_ref = m_Taxon1->GetOrgRef(tax_id, is_species,
                                          is_uncultured, blast_name);
        }

        if (org_ref) {
            m_TaxInfo[tax_id] = org_ref;
        }
    } else {
        org_ref = tax_iter->second;
    }
    return org_ref;
}


CConstRef<COrg_ref> CAlignGroup::x_GetOrgRef(const CSeq_id_Handle& id,
                                             CScope& scope)
{
    int tax_id = x_GetTaxId(id, scope);
    return x_GetOrgRef(tax_id);
}


END_NCBI_SCOPE
