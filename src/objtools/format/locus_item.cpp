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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- locus item implementation
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/general/Date.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/PDB_replace.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


CLocusItem::CLocusItem(CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_Length(0), m_Biomol(CMolInfo::eBiomol_unknown), m_Date("01-JAN-1900")
{
   x_GatherInfo(ctx);
}


void CLocusItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatLocus(*this, text_os); 
}


const string& CLocusItem::GetName(void) const
{
    return m_Name;
}


size_t CLocusItem::GetLength(void) const
{
    return m_Length;
}


CLocusItem::TStrand CLocusItem::GetStrand(void) const
{
    return m_Strand;
}


CLocusItem::TBiomol CLocusItem::GetBiomol(void) const
{
    return m_Biomol;
}


CLocusItem::TTopology CLocusItem::GetTopology (void) const
{
    return m_Topology;
}


const string& CLocusItem::GetDivision(void) const
{
    return m_Division;
}


const string& CLocusItem::GetDate(void) const
{
    return m_Date;
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CLocusItem::x_GatherInfo(CBioseqContext& ctx)
{
    CSeqdesc_CI mi_desc(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    if ( mi_desc ) {
        x_SetObject(mi_desc->GetMolinfo());
    }

    // NB: order of execution is important, as some values depend on others
    x_SetName(ctx);
    x_SetLength(ctx);
    x_SetBiomol(ctx);   // must come befoer x_SetStrand
    x_SetStrand(ctx);
    x_SetTopology(ctx);
    x_SetDivision(ctx);
    x_SetDate(ctx);
}


// Name

void CLocusItem::x_SetName(CBioseqContext& ctx)
{
    CBioseq_Handle::TBioseqCore seq  = ctx.GetHandle().GetBioseqCore();

    CConstRef<CSeq_id> id = FindBestChoice(seq->GetId(), CSeq_id::Score);
    const CTextseq_id* tsid = id->GetTextseq_Id();
    if ( tsid  &&  tsid->CanGetName() ) {
        m_Name = tsid->GetName();
    }
    if ( m_Name.empty()  ||  x_NameHasBadChars(m_Name) ) {
        m_Name = id->GetSeqIdString();
    }
}


bool CLocusItem::x_NameHasBadChars(const string& name) const
{
    ITERATE(string, iter, name) {
        if ( !isalnum(*iter) ) {
            return true;
        }
    }
    return false;
}


// Length

void CLocusItem::x_SetLength(CBioseqContext& ctx)
{
    m_Length = sequence::GetLength(ctx.GetLocation(), &ctx.GetScope());
}


// Strand

void CLocusItem::x_SetStrand(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();
    
    CSeq_inst::TMol bmol = bsh.IsSetInst_Mol() ?
        bsh.GetInst_Mol() : CSeq_inst::eMol_not_set;

    m_Strand = bsh.IsSetInst_Strand() ?
        bsh.GetInst_Strand() : CSeq_inst::eStrand_not_set;
    if ( m_Strand == CSeq_inst::eStrand_other ) {
        m_Strand = CSeq_inst::eStrand_not_set;
    }

    // cleanup for formats other than GBSeq
    if ( !ctx.Config().IsFormatGBSeq() ) {
        // if ds-DNA don't show ds
        if ( bmol == CSeq_inst::eMol_dna  &&  m_Strand == CSeq_inst::eStrand_ds ) {
            m_Strand = CSeq_inst::eStrand_not_set;
        }
        
        // ss-any RNA don't show ss
        if ( (bmol > CSeq_inst::eMol_rna  ||  
            (m_Biomol >= CMolInfo::eBiomol_mRNA     &&
            m_Biomol <= CMolInfo::eBiomol_peptide))  &&
            m_Strand == CSeq_inst::eStrand_ss ) {
            m_Strand = CSeq_inst::eStrand_not_set;
        }
    }
}


// Biomol

void CLocusItem::x_SetBiomol(CBioseqContext& ctx)
{
    if ( ctx.IsProt() ) {
        return;
    }

    CSeq_inst::TMol bmol = ctx.GetHandle().GetBioseqMolType();
    
    if ( bmol > CSeq_inst::eMol_aa ) {
        bmol = CSeq_inst::eMol_not_set;
    }

    const CMolInfo* molinfo = dynamic_cast<const CMolInfo*>(GetObject());
    if ( molinfo  &&  molinfo->GetBiomol() <= CMolInfo::eBiomol_transcribed_RNA ) {
        m_Biomol = molinfo->GetBiomol();
    }

    if ( m_Biomol <= CMolInfo::eBiomol_genomic ) {
        if ( bmol == CSeq_inst::eMol_aa ) {
            m_Biomol = CMolInfo::eBiomol_peptide;
        } else if ( bmol == CSeq_inst::eMol_na ) {
            m_Biomol = CMolInfo::eBiomol_unknown;
        } else if ( bmol == CSeq_inst::eMol_rna ) {
            m_Biomol = CMolInfo::eBiomol_pre_RNA;
        } else {
            m_Biomol = CMolInfo::eBiomol_genomic;
        }
    } else if ( m_Biomol == CMolInfo::eBiomol_other_genetic ) {
        if ( bmol == CSeq_inst::eMol_rna ) {
            m_Biomol = CMolInfo::eBiomol_pre_RNA;
        }
    }
}


// Topology

void CLocusItem::x_SetTopology(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();
    
    m_Topology = bsh.GetInst_Topology();
    // an interval is always linear
    if ( !ctx.GetLocation().IsWhole() ) {
        m_Topology = CSeq_inst::eTopology_linear;
    }
}


// Division

void CLocusItem::x_SetDivision(CBioseqContext& ctx)
{
    // contig style (old genome_view flag) forces CON division
    if ( ctx.DoContigStyle() ) {
        m_Division = "CON";
        return;
    }
    // "genome view" forces CON division
    if ( (ctx.IsSegmented()  &&  !ctx.HasParts())  ||
         (ctx.IsDelta()  &&  !ctx.IsDeltaLitOnly()) ) {
        m_Division = "CON";
        return;
    }

    const CBioseq_Handle& bsh = ctx.GetHandle();
    const CBioSource* bsrc = 0;

    // Find the BioSource object for this sequnece.
    CSeqdesc_CI src_desc(bsh, CSeqdesc::e_Source);
    if ( src_desc ) {
        bsrc = &(src_desc->GetSource());
    } else {
        CFeat_CI feat(bsh, 0, 0, CSeqFeatData::e_Biosrc);
        if ( feat ) {
            bsrc = &(feat->GetData().GetBiosrc());
        } else if ( ctx.IsProt() ) {
            // if protein with no sources, get sources applicable to 
            // DNA location of CDS
            const CSeq_feat* cds = GetCDSForProduct(bsh);
            if ( cds ) {
                CConstRef<CSeq_feat> nuc_fsrc = GetBestOverlappingFeat(
                    cds->GetLocation(),
                    CSeqFeatData::e_Biosrc,
                    eOverlap_Contains,
                    bsh.GetScope());
                if ( nuc_fsrc ) {
                    bsrc = &(nuc_fsrc->GetData().GetBiosrc());
                } else {
                    CBioseq_Handle nuc = 
                        bsh.GetScope().GetBioseqHandle(cds->GetLocation());
                    CSeqdesc_CI nuc_dsrc(nuc, CSeqdesc::e_Source);
                    if ( nuc_dsrc ) {
                        bsrc = &(nuc_dsrc->GetSource());
                    }
                }
            }
        }
    }

    bool is_transgenic = false;

    if ( bsrc ) {
        CBioSource::TOrigin origin = bsrc->GetOrigin();
        if ( bsrc->CanGetOrg() ) {
            const COrg_ref& org = bsrc->GetOrg();
            if ( org.CanGetOrgname()  &&  org.GetOrgname().CanGetDiv() ) {
                m_Division = org.GetOrgname().GetDiv();
            }
        }

        ITERATE(CBioSource::TSubtype, subsource, bsrc->GetSubtype() ) {
            if ( (*subsource)->CanGetSubtype()  &&
                (*subsource)->GetSubtype() == CSubSource::eSubtype_transgenic ) {
                is_transgenic = true;
            }
        }
        
        switch ( ctx.GetTech() ) {
        case CMolInfo::eTech_est:
            m_Division = "EST";
            break;
        case CMolInfo::eTech_sts:
            m_Division = "STS";
            break;
        case CMolInfo::eTech_survey:
            m_Division = "GSS";
            break;
        case CMolInfo::eTech_htgs_0:
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
            m_Division = "HTG";
            break;
        case CMolInfo::eTech_htc:
            m_Division = "HTC";
            break;
        default:
            break;
        }

        if ( origin == CBioSource::eOrigin_synthetic  ||  is_transgenic ) {
            m_Division = "SYN";
        }

        ITERATE( CBioseq::TId, id, bsh.GetBioseqCore()->GetId() ) {
            if ( (*id)->IdentifyAccession() == CSeq_id::eAcc_patent ) {
                m_Division = "PAT";
                break;
            }
        }

        // more complicated code for division, if necessary, goes here
        
        for ( CSeqdesc_CI gb_desc(bsh, CSeqdesc::e_Genbank); gb_desc; ++gb_desc ) {
            const CGB_block& gb = gb_desc->GetGenbank();
            if ( gb.CanGetDiv() ) {
                if ( m_Division.empty()    ||
                     gb.GetDiv() == "SYN"  ||
                     gb.GetDiv() == "PAT" ) {
                    m_Division = gb.GetDiv();
                }
            }
        }

        const CMolInfo* molinfo = dynamic_cast<const CMolInfo*>(GetObject());
        if ( ctx.Config().IsFormatEMBL() ) {
            for ( CSeqdesc_CI embl_desc(bsh, CSeqdesc::e_Embl); 
                  embl_desc; 
                  ++embl_desc ) {
                const CEMBL_block& embl = embl_desc->GetEmbl();
                if ( embl.CanGetDiv() ) {
                    if ( embl.GetDiv() == CEMBL_block::eDiv_other  &&
                         molinfo == 0 ) {
                        m_Division = "HUM";
                    } else {
                        m_Division = embl.GetDiv();
                    }
                }
            }
        }
    }

    // Set a default value (3 spaces)
    if ( m_Division.empty() ) {
        m_Division = "   ";
    }
}


// Date

void CLocusItem::x_SetDate(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();

    const CDate* date = x_GetDateForBioseq(bsh);
    if ( date == 0 ) {
        // if bioseq is product of CDS or mRNA feature, get 
        // date from parent bioseq.
        CBioseq_Handle parent = GetNucleotideParent(bsh);
        if ( parent ) {
            date = x_GetDateForBioseq(parent);
        }
    }

    if ( date ) {
        m_Date.erase();
        DateToString(*date, m_Date);
    }
}


const CDate* CLocusItem::x_GetDateForBioseq(const CBioseq_Handle& bsh) const
{
    const CDate* result = 0;

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Update_date);
        if ( desc ) {
            result = &(desc->GetUpdate_date());
        }
    }}

    {{
        // temporarily (???) also look at genbank block entry date 
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Genbank);
        if ( desc ) {
            const CGB_block& gb = desc->GetGenbank();
            if ( gb.CanGetEntry_date() ) {
                result = x_GetLaterDate(result, &gb.GetEntry_date());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Embl);
        if ( desc ) {
            const CEMBL_block& embl = desc->GetEmbl();
            if ( embl.CanGetCreation_date() ) {
                result = x_GetLaterDate(result, &embl.GetCreation_date());
            }
            if ( embl.CanGetUpdate_date() ) {
                result = x_GetLaterDate(result, &embl.GetUpdate_date());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Sp);
        if ( desc ) {
            const CSP_block& sp = desc->GetSp();
            if ( sp.CanGetCreated()  &&  sp.GetCreated().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetCreated());
            }
            if ( sp.CanGetSequpd()  &&  sp.GetSequpd().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetSequpd());
            }
            if ( sp.CanGetAnnotupd()  &&  sp.GetAnnotupd().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetAnnotupd());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Pdb);
        if ( desc ) {
            const CPDB_block& pdb = desc->GetPdb();
            if ( pdb.CanGetDeposition()  &&  pdb.GetDeposition().IsStd() ) {
                result = x_GetLaterDate(result, &pdb.GetDeposition());
            }
            if ( pdb.CanGetReplace() ) {
                const CPDB_replace& replace = pdb.GetReplace();
                if ( replace.CanGetDate()  &&   replace.GetDate().IsStd() ) {
                    result = x_GetLaterDate(result, &replace.GetDate());
                }
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Create_date);
        if ( desc ) {
            result = x_GetLaterDate(result, &(desc->GetCreate_date()));
        }
    }}

    return result;
}


const CDate* CLocusItem::x_GetLaterDate(const CDate* d1, const CDate* d2) const
{
    if ( d1 == 0 ) {
        return d2;
    }

    if ( d2 == 0 ) {
        return d1;
    }

    return (d1->Compare(*d2) == CDate::eCompare_after) ? d1 : d2;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/04/22 15:58:51  shomrat
* Changes in context
*
* Revision 1.7  2004/04/13 16:49:01  shomrat
* GBSeq format changes
*
* Revision 1.6  2004/03/25 20:45:05  shomrat
* Use handles
*
* Revision 1.5  2004/03/05 18:41:14  shomrat
* bug fix
*
* Revision 1.4  2004/02/13 14:23:08  shomrat
* force CON division for contig style and genome view
*
* Revision 1.3  2004/02/11 22:55:12  shomrat
* use IsFormatEMBL method
*
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:23:35  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
