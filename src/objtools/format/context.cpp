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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/general/Dbtag.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// CBioseqContext

// constructor
CBioseqContext::CBioseqContext
(const CBioseq_Handle& seq,
 CFlatFileContext& ffctx,
 CMasterContext* mctx) :
    m_Handle(seq),
    m_Repr(CSeq_inst::eRepr_not_set),
    m_Mol(CSeq_inst::eMol_not_set),
    m_HasParts(false),
    m_IsPart(false),
    m_PartNumber(0),
    m_IsDeltaLitOnly(false),
    m_IsProt(false),
    m_IsInGPS(false),
    m_IsInNucProt(false),
    m_IsGED(false),
    m_IsGenbank(false),
    m_IsEMBL(false),
    m_IsDDBJ(false),
    m_IsPDB(false),
    m_IsSP(false),
    m_IsTPA(false),
    m_IsJournalScan(false),
    m_IsRefSeq(false),
    m_RefseqInfo(0),
    m_IsGbGenomeProject(false),  // GenBank Genome project data (AE)
    m_IsNcbiCONDiv(false),       // NCBI CON division (CH)
    m_IsPatent(false),
    m_IsGI(false),
    m_IsWGS(false),
    m_IsWGSMaster(false),
    m_IsHup(false),
    m_Gi(0),
    m_ShowGBBSource(false),
    m_PatSeqid(0),
    m_HasOperon(false),
    m_FFCtx(ffctx),
    m_Master(mctx)
{
    x_Init(seq, m_FFCtx.GetLocation());
}


// destructor
CBioseqContext::~CBioseqContext(void)
{
    if (m_Virtual) {
        m_Virtual.GetEditHandle().Remove();
    }
}


const CSeq_id& CBioseqContext::GetPreferredSynonym(const CSeq_id& id) const
{
    if ( id.IsGi()  &&  id.GetGi() == m_Gi ) {
        return *m_PrimaryId;
    }

    CBioseq_Handle h = m_Handle.GetScope().GetBioseqHandleFromTSE(id, m_Handle);
    if ( h ) {
        if ( h == m_Handle ) {
            return *m_PrimaryId;
        } else if ( h.GetSeqId().NotEmpty() ) {
            return *FindBestChoice(h.GetBioseqCore()->GetId(), CSeq_id::Score);
        }
    }

    return id;
}



// initialization
void CBioseqContext::x_Init(const CBioseq_Handle& seq, const CSeq_loc* user_loc)
{
    _ASSERT(seq);
    _ASSERT(seq.IsSetInst());

    // NB: order of execution is important
    x_SetId();
    m_Repr = x_GetRepr();
    m_Mol  = seq.GetInst_Mol();
    m_Molinfo.Reset(x_GetMolInfo());

    if ( IsSegmented() ) {
        m_HasParts = x_HasParts();
    }
    m_IsPart = x_IsPart();
    if ( m_IsPart ) {
        _ASSERT(m_Master);
        m_PartNumber = x_GetPartNumber();
    }
    if ( IsDelta() ) {
        m_IsDeltaLitOnly = x_IsDeltaLitOnly();
    }

    m_IsProt = CSeq_inst::IsAa(seq.GetInst_Mol());

    m_IsInGPS     = x_IsInGPS();
    m_IsInNucProt = x_IsInNucProt();

    x_SetLocation(user_loc);
    
    m_Encode.Reset(x_GetEncode());
    
    m_HasOperon = x_HasOperon();
}


void CBioseqContext::x_SetLocation(const CSeq_loc* user_loc)
{
    CRef<CSeq_loc> loc;

    if (user_loc != NULL) {
        // map the user location to the current bioseq
        CSeq_loc_Mapper mapper(m_Handle, CSeq_loc_Mapper::eSeqMap_Up);
        loc.Reset(mapper.Map(*user_loc));

        if (loc) {
            if (loc->IsWhole()) {
                loc.Reset();
            } else if (loc->IsInt()) {
                CSeq_loc::TRange range = loc->GetTotalRange();
                if (range.GetFrom() == 0  &&  range.GetTo() == m_Handle.GetInst_Length() - 1) {
                    loc.Reset();
                }
            }
        }
    }

    // if no partial location specified do the entire bioseq
    if (!loc) {
        loc.Reset(new CSeq_loc);
        loc->SetWhole(*m_PrimaryId);
    } else {
        x_SetMapper(*loc);
    }

    m_Location = loc;
}


void CBioseqContext::x_SetMapper(const CSeq_loc& loc)
{
    _ASSERT(GetBioseqFromSeqLoc(loc, GetScope()) == m_Handle);

    // not covering the entire bioseq (may be multiple ranges)
    CRef<CBioseq> vseq(new CBioseq(loc, GetAccession()));
    vseq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    CBioseq_Handle vseqh = GetScope().AddBioseq(*vseq);

    if (vseqh) {
        m_Mapper.Reset(new CSeq_loc_Mapper(vseqh, CSeq_loc_Mapper::eSeqMap_Up));
        m_Mapper->SetMergeAbutting();
        m_Mapper->SetGapRemove();
        //m_Mapper->KeepNonmappingRanges();
    }
}


bool CBioseqContext::x_HasOperon(void) const
{
    return CFeat_CI(m_Handle.GetScope(),
                    *m_Location,
                    CSeqFeatData::eSubtype_operon);
}


void CBioseqContext::x_SetId(void)
{
    m_PrimaryId.Reset(new CSeq_id);
    m_PrimaryId->Assign(*sequence::GetId(
        m_Handle, sequence::eGetId_Best).GetSeqId());

    m_Accession.erase();
    m_PrimaryId->GetLabel(&m_Accession, CSeq_id::eContent);

    ITERATE (CBioseq::TId, id_iter, m_Handle.GetBioseqCore()->GetId()) {
        const CSeq_id& id = **id_iter;
        const CTextseq_id* tsip = id.GetTextseq_Id();
        const string& acc = (tsip != 0  &&  tsip->CanGetAccession()) ?
            tsip->GetAccession() : kEmptyStr;

        CSeq_id::EAccessionInfo acc_info = id.IdentifyAccession();
        unsigned int acc_type = acc_info & CSeq_id::eAcc_type_mask;
        unsigned int acc_div =  acc_info & CSeq_id::eAcc_division_mask;

        switch ( id.Which() ) {
        // Genbank, Embl or Ddbj
        case CSeq_id::e_Embl:
            m_IsEMBL = true;
            break;
        case CSeq_id::e_Ddbj:
            m_IsDDBJ = true;
            break;
        case CSeq_id::e_Genbank:
            m_IsGenbank = true;
            m_IsGbGenomeProject = m_IsGbGenomeProject  ||
                ((acc_type & CSeq_id::eAcc_gb_genome) != 0);
            m_IsNcbiCONDiv = m_IsNcbiCONDiv  ||
                ((acc_type & CSeq_id::eAcc_gb_con) != 0);
            break;
        // Patent
        case CSeq_id::e_Patent:
            m_IsPatent = true;
            if (id.GetPatent().IsSetSeqid()) {
                m_PatSeqid = id.GetPatent().GetSeqid();
            }
            break;
        // RefSeq
        case CSeq_id::e_Other:
            m_IsRefSeq = true;
            m_RefseqInfo = acc_info;
            break;
        // Gi
        case CSeq_id::e_Gi:
            m_IsGI = true;
            m_Gi = id.GetGi();
            break;
        // PDB
        case CSeq_id::e_Pdb:
            m_IsPDB = true;
            break;
        // TPA
        case CSeq_id::e_Tpg:
            m_IsTPA = true;
            m_IsGenbank = true;
            break;
        case CSeq_id::e_Tpe:
            m_IsTPA = true;
            m_IsEMBL = true;
            break;
        case CSeq_id::e_Tpd:
            m_IsTPA = true;
            m_IsDDBJ = true;
            break;
        case CSeq_id::e_General:
            if ( id.GetGeneral().CanGetDb() ) {
                if ( !NStr::CompareCase(id.GetGeneral().GetDb(), "BankIt") ) {
                    m_IsTPA = true;
                }
            }
            break;
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
            m_IsJournalScan = true;
            break;
        case CSeq_id::e_Swissprot:
            m_IsSP = true;
            break;
        // nothing special
        case CSeq_id::e_Pir:
        case CSeq_id::e_not_set:
        case CSeq_id::e_Local:
        case CSeq_id::e_Prf:
        default:
            break;
        }

        // WGS
        m_IsWGS = m_IsWGS  ||  (acc_div == CSeq_id::eAcc_wgs);
        
        if ( m_IsWGS  &&  !acc.empty() ) {
            size_t len = acc.length();
            m_IsWGSMaster = 
                ((len == 12  ||  len == 15)  &&  NStr::EndsWith(acc, "000000"))  ||
                (len == 13  &&  NStr::EndsWith(acc, "0000000"));
            if ( m_IsWGSMaster ) {
                m_WGSMasterAccn = acc;
                m_WGSMasterName = tsip->CanGetName() ? tsip->GetName() : kEmptyStr;
            }
        } 

        // GBB source
        m_ShowGBBSource = m_ShowGBBSource  ||  (acc_info == CSeq_id::eAcc_gsdb_dirsub);
    }

    // Genbank/Embl/Ddbj (GED)
    m_IsGED = m_IsEMBL  ||  m_IsDDBJ  ||  m_IsGenbank;
}


CSeq_inst::TRepr CBioseqContext::x_GetRepr(void) const
{
    return m_Handle.IsSetInst_Repr() ?
        m_Handle.GetInst_Repr() : CSeq_inst::eRepr_not_set;
}


const CMolInfo* CBioseqContext::x_GetMolInfo(void) const
{
    CSeqdesc_CI desc(m_Handle, CSeqdesc::e_Molinfo);
    return desc ? &desc->GetMolinfo() : 0;
}


bool CBioseqContext::x_IsPart() const
{
    if ( m_Repr == CSeq_inst::eRepr_raw    ||
         m_Repr == CSeq_inst::eRepr_const  ||
         m_Repr == CSeq_inst::eRepr_delta  ||
         m_Repr == CSeq_inst::eRepr_virtual ) {
         const CSeq_entry_Handle& fftse = GetTopLevelEntry();
         CSeq_entry_Handle eh = m_Handle.GetParentEntry();
         _ASSERT(eh  &&  eh.IsSeq());

         if (fftse != eh) {
             eh = eh.GetParentEntry();
             if ( eh  &&  eh.IsSet() ) {
                 CBioseq_set_Handle bsst = eh.GetSet();
                 if ( bsst.IsSetClass()  &&
                     bsst.GetClass() == CBioseq_set::eClass_parts ) {
                     return true;
                 }
            }
        }
    }
    return false;
}


bool CBioseqContext::x_HasParts(void) const
{
    _ASSERT(IsSegmented());

    CSeq_entry_Handle h =
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if ( !h ) {
        return false;
    }
    
    // make sure the segmented set contains our bioseq
    {{
        bool has_seq = false;
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSeq()  &&  it->GetSeq() == m_Handle ) {
                has_seq = true;
                break;
            }
        }
        if ( !has_seq ) {
            return false;
        }
    }}

    // find the parts set
    {{
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSet()  &&  it->GetSet().IsSetClass()  &&
                it->GetSet().GetClass() == CBioseq_set::eClass_parts ) {
                return true;
            }
        }
    }}

    return false;
}


bool CBioseqContext::x_IsDeltaLitOnly(void) const
{
    _ASSERT(IsDelta());

    if ( m_Handle.IsSetInst_Ext() ) {
        const CBioseq_Handle::TInst_Ext& ext = m_Handle.GetInst_Ext();
        if ( ext.IsDelta() ) {
            ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                if ( (*it)->IsLoc() ) {
                    return false;
                }
            }
        }
    }
    return true;
}


SIZE_TYPE CBioseqContext::x_GetPartNumber(void)
{
    return m_Master ? m_Master->GetPartNumber(m_Handle) : 0;
}


bool CBioseqContext::x_IsInGPS(void) const
{
    CSeq_entry_Handle e = 
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
    return e;
}


bool CBioseqContext::x_IsInNucProt(void) const
{
    CSeq_entry_Handle e = 
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
    return e;
}


bool CBioseqContext::DoContigStyle(void) const
{
    const CFlatFileConfig& cfg = Config();
    if ( cfg.IsStyleContig() ) {
        return true;
    } else if ( cfg.IsStyleNormal() ) {
        if ( (IsSegmented()  &&  !HasParts())  ||
             (IsDelta()  &&  !IsDeltaLitOnly()) ) {
            return true;
        }
    }

    return false;
}


const CUser_object* CBioseqContext::x_GetEncode(void) const
{
    for (CSeqdesc_CI it(m_Handle, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()) {
            if (NStr::EqualNocase(uo.GetType().GetStr(), "ENCODE")) {
                return &uo;
            }
        }
    }
    return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
// CMasterContext


CMasterContext::CMasterContext(const CBioseq_Handle& seq) :
    m_Handle(seq)
{
    _ASSERT(seq);
    _ASSERT(seq.GetInst_Ext().IsSeg());

    x_SetNumParts();
    x_SetBaseName();
}


CMasterContext::~CMasterContext(void)
{
}


SIZE_TYPE CMasterContext::GetPartNumber(const CBioseq_Handle& part)
{
    if ( !part ) {
        return 0;
    }
    CScope& scope = m_Handle.GetScope();

    SIZE_TYPE serial = 1;
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        if ((*it)->IsNull()) {
            continue;
        }
        const CSeq_id& id = GetId(**it, &m_Handle.GetScope());
        CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, m_Handle);
        if (bsh  &&
            bsh.IsSetInst_Repr()  &&
            bsh.GetInst_Repr() != CSeq_inst::eRepr_virtual) {
            if (bsh == part) {
                return serial;
            }
            ++serial;
        }
    }

    return 0;
}


void CMasterContext::x_SetNumParts(void)
{
    CScope& scope = m_Handle.GetScope();
    SIZE_TYPE count = 0;

    // count only non-gap and non-virtual parts
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        const CSeq_loc& loc = **it;
        if (loc.IsNull()) { // skip gaps
            continue;
        }
        // count only non-virtual
        const CSeq_id_Handle id = CSeq_id_Handle::GetHandle(GetId(loc, &scope));
        CBioseq_Handle part = scope.GetBioseqHandleFromTSE(id, m_Handle);
        if (part  &&
            part.IsSetInst_Repr()  &&
            part.GetInst_Repr() != CSeq_inst::eRepr_virtual) {
            ++count;    
        }
    }
    m_NumParts = count;
}


static void s_GetNameForBioseq(const CBioseq_Handle& seq, string& name)
{
    name.erase();

    CConstRef<CSeq_id> sip;
    ITERATE (CBioseq_Handle::TId, it, seq.GetId()) {
        CConstRef<CSeq_id> id = it->GetSeqId();
        if (id->IsGenbank()  ||  id->IsEmbl()    ||  id->IsDdbj()  ||
            id->IsTpg()      ||  id->IsTpe()     ||  id->IsTpd()) {
            sip = id;
            break;
        }
    }

    if (sip) {
        const CTextseq_id* tsip = sip->GetTextseq_Id();
        if (tsip != NULL  &&  tsip->CanGetName()) {
            name = tsip->GetName();
        }
    }
}


void CMasterContext::x_SetBaseName(void)
{
    string parent_name;
    s_GetNameForBioseq(m_Handle, parent_name);

    // if there's no "SEG_" prefix just use the master's name
    if (!NStr::StartsWith(parent_name, "SEG_")) {
        m_BaseName = parent_name;
        return;
    }

    // otherwise, eliminate the prefix ...
    parent_name = parent_name.substr(4);

    // ... and calculate a base name

    // find the first segment
    CScope* scope = &m_Handle.GetScope();
    CBioseq_Handle segment;
    const CSeqMap& seqmap = m_Handle.GetSeqMap();
    CSeqMap_CI it = seqmap.BeginResolved(scope,
                                         SSeqMapSelector()
                                         .SetResolveCount(1)
                                         .SetFlags(CSeqMap::fFindRef));
    while (it) {
        CSeq_id_Handle id = it.GetRefSeqid();
        segment = scope->GetBioseqHandleFromTSE(id, m_Handle);
        if (segment) {
            break;
        }
    }
    string seg_name;
    if (segment) {
        s_GetNameForBioseq(segment, seg_name);
    }

    if (!seg_name.empty()  &&  NStr::EndsWith(seg_name, '1')  &&
        parent_name.length() == seg_name.length()  &&
        NStr::EndsWith(parent_name, '1')) {
        for (size_t pos = parent_name.length() - 2; pos >= 0; --pos) {
            if (parent_name[pos] != '0') {
                break;
            }
            parent_name.erase(pos + 1);
        }
    }

    m_BaseName = parent_name;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.38  2005/04/18 13:49:34  shomrat
* Added support for ENCODE project
*
* Revision 1.37  2005/03/28 17:18:39  shomrat
* Support for complex user location
*
* Revision 1.36  2005/03/14 18:19:02  grichenk
* Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
* SAnnotSelector.
*
* Revision 1.35  2005/03/07 17:17:27  shomrat
* Check if a Bioseq has an operon feature
*
* Revision 1.34  2005/02/18 15:08:08  shomrat
* CSeq_loc interface changes
*
* Revision 1.33  2005/02/17 15:58:42  grichenk
* Changes sequence::GetId() to return CSeq_id_Handle
*
* Revision 1.32  2005/02/01 21:55:11  grichenk
* Added direction flag for mapping between top level sequence
* and segments.
*
* Revision 1.31  2005/01/12 15:17:12  shomrat
* Added GetPatentSeqId
*
* Revision 1.30  2004/12/14 17:41:03  grichenk
* Reduced number of CSeqMap::FindResolved() methods, simplified
* BeginResolved and EndResolved. Marked old methods as deprecated.
*
* Revision 1.29  2004/11/24 16:50:21  shomrat
* + IsGenbank
*
* Revision 1.28  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.27  2004/11/15 20:06:00  shomrat
* Flag EMBL/DDBJ for EMBL and DDBJ TPAs
*
* Revision 1.26  2004/10/22 15:41:34  shomrat
* + IsDDBJ
*
* Revision 1.25  2004/10/18 18:42:52  shomrat
* Indicate if scanned from journal
*
* Revision 1.24  2004/10/05 15:38:11  shomrat
* Use more efficient NStr::EndsWith function
*
* Revision 1.23  2004/09/01 19:56:36  shomrat
* fixed intializaion of ShowGBBSource flag
*
* Revision 1.22  2004/09/01 15:33:44  grichenk
* Check strand in GetStart and GetEnd. Circular length argument
* made optional.
*
* Revision 1.21  2004/08/25 15:03:56  grichenk
* Removed duplicate methods from CSeqMap
*
* Revision 1.20  2004/08/19 20:32:30  ucko
* Substitute string::erase() for string::clear(), which still isn't
* portable (to gcc 2.95).
*
* Revision 1.19  2004/08/19 16:28:45  shomrat
* Implemented GetBaseName
*
* Revision 1.18  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.17  2004/05/08 12:11:39  dicuccio
* Use CSeq_id::GetLabel() instead of GetSeqIdString()
*
* Revision 1.16  2004/05/06 17:48:49  shomrat
* + IsEMBL and IsInNucProt
*
* Revision 1.15  2004/04/27 15:12:24  shomrat
* Added logic for partial range formatting
*
* Revision 1.14  2004/04/22 15:52:00  shomrat
* Refactring of context
*
* Revision 1.13  2004/03/31 17:15:09  shomrat
* name changes
*
* Revision 1.12  2004/03/26 17:23:38  shomrat
* fixed initialization of m_IsWGS
*
* Revision 1.11  2004/03/25 20:36:02  shomrat
* Use handles
*
* Revision 1.10  2004/03/24 18:57:04  vasilche
* CBioseq_Handle::GetComplexityLevel() now returns CSeq_entry_Handle.
*
* Revision 1.9  2004/03/18 15:36:26  shomrat
* + new flag ShowFtableRefs
*
* Revision 1.8  2004/03/10 21:25:42  shomrat
* Fix m_RefseqInfo initialization
*
* Revision 1.7  2004/03/05 18:46:26  shomrat
* Added customization flags
*
* Revision 1.6  2004/02/19 18:03:29  shomrat
* changed implementation of GetPreferredSynonym
*
* Revision 1.5  2004/02/11 22:49:04  shomrat
* using types in flag file
*
* Revision 1.4  2004/02/11 16:49:16  shomrat
* added user customization flags
*
* Revision 1.3  2004/01/14 16:09:04  shomrat
* multiple changes (work in progress)
*
* Revision 1.2  2003/12/18 17:43:31  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:18:44  shomrat
* Initial Revision (adapted from flat lib)
*
* Revision 1.4  2003/06/02 16:06:42  dicuccio
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
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
