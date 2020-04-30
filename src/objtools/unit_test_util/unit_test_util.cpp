/* $Id$
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates objects::CSeq_entries and objects::CSeq_submits
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>

#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(unit_test_util)


void SetDbxref (objects::CBioSource& src, string db, objects::CObject_id::TId id)
{
    CRef<objects::CDbtag> dbtag(new objects::CDbtag());
    dbtag->SetDb(db);
    dbtag->SetTag().SetId(id);
    src.SetOrg().SetDb().push_back(dbtag);
}


void RemoveDbxref (objects::CBioSource& src, string db, objects::CObject_id::TId id)
{
    if (src.IsSetOrg() && src.GetOrg().IsSetDb()) {
        objects::COrg_ref::TDb::iterator it = src.SetOrg().SetDb().begin();
        while (it != src.SetOrg().SetDb().end()) {
            if ((NStr::IsBlank(db) || ((*it)->IsSetDb() && NStr::Equal((*it)->GetDb(), db)))
                && (id == 0 || ((*it)->IsSetTag() && (*it)->GetTag().IsId() && (*it)->GetTag().GetId() == id))) {
                it = src.SetOrg().SetDb().erase(it);
            } else {
                ++it;
            }
        }
    }
}


void SetTaxon (objects::CBioSource& src, size_t taxon)
{
    if (taxon == 0) {
        RemoveDbxref (src, "taxon", 0);
    } else {
        SetDbxref(src, "taxon", taxon);
    }
}


CRef<objects::CSeq_entry> BuildGoodSeq(void)
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    entry->SetSeq().SetInst().SetLength(60);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr ("good");
    entry->SetSeq().SetId().push_back(id);

    CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_genomic);    
    entry->SetSeq().SetDescr().Set().push_back(mdesc);

    AddGoodSource (entry);
    AddGoodPub(entry);

    return entry;
}


CRef<objects::CSeqdesc> BuildGoodPubSeqdesc()
{
    CRef<objects::CSeqdesc> pdesc(new objects::CSeqdesc());
    CRef<objects::CPub> pub(new objects::CPub());
    pub->SetPmid(CPub::TPmid(ENTREZ_ID_CONST(1)));
    pdesc->SetPub().SetPub().Set().push_back(pub);

    return pdesc;
}


void AddGoodPub (CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeqdesc> pdesc = BuildGoodPubSeqdesc();

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(pdesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(pdesc);
    }

    CRef<objects::CSeqdesc> pdesc2 = BuildGoodPubSeqdesc();
    pdesc2->SetPub().SetPub().Set().front()->Assign(*BuildGoodCitSubPub());
    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(pdesc2);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(pdesc2);
    }

}


void AddGoodSource (CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeqdesc> odesc(new objects::CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname("Sebaea microphylla");
    odesc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    SetTaxon(odesc->SetSource(), 592768);
    CRef<objects::CSubSource> subsrc(new objects::CSubSource());
    subsrc->SetSubtype(objects::CSubSource::eSubtype_chromosome);
    subsrc->SetName("1");
    odesc->SetSource().SetSubtype().push_back(subsrc);

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(odesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(odesc);
    }
}


void SetDbxref (objects::CBioSource& src, string db, string id)
{
    CRef<objects::CDbtag> dbtag(new objects::CDbtag());
    dbtag->SetDb(db);
    dbtag->SetTag().SetStr(id);
    src.SetOrg().SetDb().push_back(dbtag);
}


void SetDbxref (CRef<objects::CSeq_entry> entry, string db, objects::CObject_id::TId id)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetDbxref((*it)->SetSource(), db, id);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetDbxref((*it)->SetSource(), db, id);
            }
        }
    }
}


void SetDbxref (CRef<objects::CSeq_entry> entry, string db, string id)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetDbxref((*it)->SetSource(), db, id);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetDbxref((*it)->SetSource(), db, id);
            }
        }
    }
}


void SetDbxref (CRef<objects::CSeq_feat> feat, string db, objects::CObject_id::TId id)
{
    if (!feat) {
        return;
    }
    CRef<objects::CDbtag> dbtag(new objects::CDbtag());
    dbtag->SetDb(db);
    dbtag->SetTag().SetId(id);
    feat->SetDbxref().push_back(dbtag);
}


void SetDbxref (CRef<objects::CSeq_feat> feat, string db, string id)
{
    if (!feat) {
        return;
    }
    CRef<objects::CDbtag> dbtag(new objects::CDbtag());
    dbtag->SetDb(db);
    dbtag->SetTag().SetStr(id);
    feat->SetDbxref().push_back(dbtag);
}



void RemoveDbxref (CRef<objects::CSeq_entry> entry, string db, objects::CObject_id::TId id)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                RemoveDbxref((*it)->SetSource(), db, id);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                RemoveDbxref((*it)->SetSource(), db, id);
            }
        }
    }
}


void RemoveDbxref (CRef<objects::CSeq_feat> feat, string db, objects::CObject_id::TId id)
{
    if (!feat) {
        return;
    }
    if (feat->IsSetDbxref()) {
        objects::CSeq_feat::TDbxref::iterator it = feat->SetDbxref().begin();
        while (it != feat->SetDbxref().end()) {
            if ((NStr::IsBlank(db) || ((*it)->IsSetDb() && NStr::Equal((*it)->GetDb(), db))) 
                && (id == 0 || ((*it)->IsSetTag() && (*it)->GetTag().IsId() && (*it)->GetTag().GetId() == id))) {
                it = feat->SetDbxref().erase(it);
            } else {
                ++it;
            }
        }
    }
}


void SetTaxon (CRef<objects::CSeq_entry> entry, size_t taxon)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTaxon((*it)->SetSource(), taxon);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTaxon((*it)->SetSource(), taxon);
            }
        }
    }
}


void AddFeatAnnotToSeqEntry (CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_entry> entry)
{
    if (!entry || !annot) {
        return;
    }
    if (entry->IsSeq()) {
        entry->SetSeq().SetAnnot().push_back(annot);
    } else if (entry->IsSet()) {
        if (entry->GetSet().IsSetSeq_set()) {
            AddFeatAnnotToSeqEntry (annot, entry->SetSet().SetSeq_set().front());
        }
    }
}


CRef<objects::CSeq_annot> AddFeat (CRef<objects::CSeq_feat> feat, CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_annot> annot;

    if (entry->IsSeq()) {
        if (!entry->GetSeq().IsSetAnnot() 
            || !entry->GetSeq().GetAnnot().front()->IsFtable()) {
            CRef<objects::CSeq_annot> new_annot(new objects::CSeq_annot());
            entry->SetSeq().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSeq().SetAnnot().front();
        }
    } else if (entry->IsSet()) {
        if (!entry->GetSet().IsSetAnnot() 
            || !entry->GetSet().GetAnnot().front()->IsFtable()) {
            CRef<objects::CSeq_annot> new_annot(new objects::CSeq_annot());
            entry->SetSet().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSet().SetAnnot().front();
        }
    }
    annot->SetData().SetFtable().push_back(feat);
    return annot;
}

CRef<objects::CSeq_feat> AddProtFeat(CRef<objects::CSeq_entry> entry) 
{
    CRef<objects::CSeq_feat> feat (new objects::CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    AddFeat (feat, entry);
    return feat;
}


CRef<objects::CSeq_feat> AddGoodSourceFeature(CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat());
    feat->SetData().SetBiosrc().SetOrg().SetTaxname("Trichechus manatus");
    SetTaxon (feat->SetData().SetBiosrc(), 9778);
    feat->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    AddFeatAnnotToSeqEntry (annot, entry);
    return feat;
}


CRef<objects::CSeq_feat> MakeMiscFeature(CRef<objects::CSeq_id> id, size_t right_end, size_t left_end)
{
    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat());
    feat->SetLocation().SetInt().SetId().Assign(*id);
    feat->SetLocation().SetInt().SetFrom(left_end);
    feat->SetLocation().SetInt().SetTo(right_end);
    feat->SetData().SetImp().SetKey("misc_feature");
    return feat;
}


CRef<CSeq_feat> BuildGoodFeat ()
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(59);
    feat->SetData().SetImp().SetKey("misc_feature");

    return feat;
}


CRef<objects::CSeq_id> IdFromEntry(CRef<objects::CSeq_entry> entry)
{
    if (entry->IsSeq()) {
        return entry->SetSeq().SetId().front();
    } else if (entry->IsSet()) {
        return IdFromEntry (entry->SetSet().SetSeq_set().front());
    } else {
        CRef<objects::CSeq_id> empty;
        return empty;
    }
}


CRef<objects::CSeq_feat> AddMiscFeature(CRef<objects::CSeq_entry> entry, size_t right_end)
{
    CRef<objects::CSeq_feat> feat = MakeMiscFeature(IdFromEntry(entry), right_end);
    feat->SetComment("misc_feature needs a comment");
    AddFeat (feat, entry);
    return feat;
}


CRef<objects::CSeq_feat> AddMiscFeature(CRef<objects::CSeq_entry> entry)
{
    return AddMiscFeature (entry, 10);
}


void SetTaxname (CRef<objects::CSeq_entry> entry, string taxname)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(taxname)) {
                    (*it)->SetSource().SetOrg().ResetTaxname();
                } else {
                    (*it)->SetSource().SetOrg().SetTaxname(taxname);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(taxname)) {
                    (*it)->SetSource().SetOrg().ResetTaxname();
                } else {
                    (*it)->SetSource().SetOrg().SetTaxname(taxname);
                }
            }
        }
    }
}


void SetSebaea_microphylla(CRef<objects::CSeq_entry> entry)
{
    SetTaxname(entry, "Sebaea microphylla");
    SetTaxon(entry, 0);
    SetTaxon(entry, 592768);
}


void SetSynthetic_construct(CRef<objects::CSeq_entry> entry)
{
    SetTaxname(entry, "synthetic construct");
    SetTaxon(entry, 0);
    SetTaxon(entry, 32630);
}


void SetDrosophila_melanogaster(CRef<objects::CSeq_entry> entry)
{
    SetTaxname(entry, "Drosophila melanogaster");
    SetTaxon(entry, 0);
    SetTaxon(entry, 7227);
}

void SetCommon (CRef<objects::CSeq_entry> entry, string common)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(common)) {
                    (*it)->SetSource().SetOrg().ResetCommon();
                } else {
                    (*it)->SetSource().SetOrg().SetCommon(common);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(common)) {
                    (*it)->SetSource().SetOrg().ResetCommon();
                } else {
                    (*it)->SetSource().SetOrg().SetCommon(common);
                }
            }
        }
    }
}


void SetLineage (CRef<objects::CSeq_entry> entry, string lineage)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(lineage)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetLineage();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetLineage(lineage);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(lineage)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetLineage();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetLineage(lineage);
                }
            }
        }
    }
}


void SetDiv (CRef<objects::CSeq_entry> entry, string div)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(div)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetDiv();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetDiv(div);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(div)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetDiv();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetDiv(div);
                }
            }
        }
    }
}


void SetOrigin (CRef<objects::CSeq_entry> entry, objects::CBioSource::TOrigin origin)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrigin(origin);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrigin(origin);
            }
        }
    }
}


void SetGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode gcode)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetGcode(gcode);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetGcode(gcode);
            }
        }
    }
}


void SetMGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode mgcode)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetMgcode(mgcode);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetMgcode(mgcode);
            }
        }
    }
}


void SetPGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode pgcode)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetPgcode(pgcode);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().SetOrgname().SetPgcode(pgcode);
            }
        }
    }
}


void ResetOrgname (CRef<objects::CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().ResetOrgname();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().ResetOrgname();
            }
        }
    }
}


void SetFocus (CRef<objects::CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetIs_focus();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetIs_focus();
            }
        }
    }
}


void ClearFocus (CRef<objects::CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().ResetIs_focus();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().ResetIs_focus();
            }
        }
    }
}


void SetGenome (CRef<objects::CSeq_entry> entry, objects::CBioSource::TGenome genome)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetGenome(genome);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetGenome(genome);
            }
        }
    }
}


void SetSubSource (objects::CBioSource& src, objects::CSubSource::TSubtype subtype, string val)
{
    if (NStr::IsBlank(val)) {
        if (src.IsSetSubtype()) {
            objects::CBioSource::TSubtype::iterator it = src.SetSubtype().begin();
            while (it != src.SetSubtype().end()) {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
                    it = src.SetSubtype().erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        CRef<objects::CSubSource> sub(new objects::CSubSource(subtype, val));
        if (NStr::EqualNocase(val, "true")) {
            sub->SetName("");
        }
        src.SetSubtype().push_back(sub);
    }
}


void SetSubSource (CRef<objects::CSeq_entry> entry, objects::CSubSource::TSubtype subtype, string val)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetSubSource((*it)->SetSource(), subtype, val);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetSubSource((*it)->SetSource(), subtype, val);
            }
        }
    }
}


void SetChromosome (objects::CBioSource& src, string chromosome) 
{
    if (NStr::IsBlank(chromosome)) {
        if (src.IsSetSubtype()) {
            objects::CBioSource::TSubtype::iterator it = src.SetSubtype().begin();
            while (it != src.SetSubtype().end()) {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == objects::CSubSource::eSubtype_chromosome) {
                    it = src.SetSubtype().erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        CRef<objects::CSubSource> sub(new objects::CSubSource(objects::CSubSource::eSubtype_chromosome, chromosome));
        src.SetSubtype().push_back(sub);
    }
}


void SetChromosome (CRef<objects::CSeq_entry> entry, string chromosome)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetChromosome((*it)->SetSource(), chromosome);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetChromosome((*it)->SetSource(), chromosome);
            }
        }
    }
}


void SetTransgenic (objects::CBioSource& src, bool do_set) 
{
    if (do_set) {
        CRef<objects::CSubSource> sub(new objects::CSubSource(objects::CSubSource::eSubtype_transgenic, ""));
        src.SetSubtype().push_back(sub);
    } else if (src.IsSetSubtype()) {
        objects::CBioSource::TSubtype::iterator it = src.SetSubtype().begin();
        while (it != src.SetSubtype().end()) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == objects::CSubSource::eSubtype_transgenic) {
                it = src.SetSubtype().erase(it);
            } else {
                ++it;
            }
        }
    }
}


void SetTransgenic (CRef<objects::CSeq_entry> entry, bool do_set)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTransgenic((*it)->SetSource(), do_set);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTransgenic((*it)->SetSource(), do_set);
            }
        }
    }
}


void SetOrgMod (objects::CBioSource& src, objects::COrgMod::TSubtype subtype, string val)
{
    if (NStr::IsBlank(val)) {
        if (src.IsSetOrg() && src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetMod()) {
            objects::COrgName::TMod::iterator it = src.SetOrg().SetOrgname().SetMod().begin();
            while (it != src.SetOrg().SetOrgname().SetMod().end()) {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
                    it = src.SetOrg().SetOrgname().SetMod().erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        CRef<objects::COrgMod> sub(new objects::COrgMod(subtype, val));
        src.SetOrg().SetOrgname().SetMod().push_back(sub);
    }
}


void SetOrgMod (CRef<objects::CSeq_entry> entry, objects::COrgMod::TSubtype subtype, string val)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetOrgMod((*it)->SetSource(), subtype, val);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetOrgMod((*it)->SetSource(), subtype, val);
            }
        }
    }
}


CRef<objects::CAuthor> BuildGoodAuthor()
{
    CRef<objects::CAuthor> author(new objects::CAuthor());
    author->SetName().SetName().SetLast("Last");
    author->SetName().SetName().SetFirst("First");
    author->SetName().SetName().SetMiddle("M");
    return author;
}


CRef<objects::CPub> BuildGoodArticlePub()
{
    CRef<objects::CPub> pub(new objects::CPub());

    CRef<objects::CCit_art::TTitle::C_E> art_title(new objects::CCit_art::TTitle::C_E());
    art_title->SetName("article title");
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    CRef<objects::CCit_jour::TTitle::C_E> journal_title(new objects::CCit_jour::TTitle::C_E());
    journal_title->SetName("journal_title");
    pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().push_back(journal_title);
    CRef<objects::CCit_jour::TTitle::C_E> iso_jta(new objects::CCit_jour::TTitle::C_E());   
    iso_jta->SetIso_jta("abbr");
    pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().push_back(iso_jta);
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(BuildGoodAuthor());
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetVolume("vol 1");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-32");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetDate().SetStd().SetYear(2009);
    return pub;
}


CRef<objects::CPub> BuildGoodCitGenPub(CRef<objects::CAuthor> author, int serial_number)
{
    CRef<objects::CPub> pub(new objects::CPub());
    if (!author) {
        author = BuildGoodAuthor();
    }
    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    if (serial_number > -1) {
        pub->SetGen().SetSerial_number(serial_number);
    }
    return pub;
}


CRef<objects::CPub> BuildGoodCitSubPub()
{
    CRef<objects::CPub> pub(new objects::CPub());
    CRef<objects::CAuthor> author = BuildGoodAuthor();
    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetSub().SetAuthors().SetAffil().SetStd().SetAffil("A Major University");
    pub->SetSub().SetAuthors().SetAffil().SetStd().SetSub("Maryland");
    pub->SetSub().SetAuthors().SetAffil().SetStd().SetCountry("USA");
    pub->SetSub().SetDate().SetStd().SetYear(2009);
    return pub;
}


void MakeSeqLong(objects::CBioseq& seq)
{
    if (seq.SetInst().IsSetSeq_data()) {
        if (seq.GetInst().GetSeq_data().IsIupacna()) {
            seq.SetInst().SetSeq_data().SetIupacna().Set().clear();
            for (int i = 0; i < 100; i++) {
                seq.SetInst().SetSeq_data().SetIupacna().Set().append(
                    "AAAAATTTTTGGGGGCCCCCTTTTTAAAAATTTTTGGGGGCCCCCTTTTTAAAAATTTTTGGGGGCCCCCTTTTTAAAAATTTTTGGGGGCCCCCTTTTT");
            }
            seq.SetInst().SetLength(10000);
        } else if (seq.GetInst().GetSeq_data().IsIupacaa()) {
            seq.SetInst().SetSeq_data().SetIupacaa().Set().clear();
            for (int i = 0; i < 100; i++) {
                seq.SetInst().SetSeq_data().SetIupacaa().Set().append(
                    "MPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSLMPRKTEINSL");
            }
            seq.SetInst().SetLength(10000);
        }
    }
}


void SetBiomol (CRef<objects::CSeq_entry> entry, objects::CMolInfo::TBiomol biomol)
{
    bool found = false;

    NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(biomol);
            found = true;
        }
    }
    if (!found) {
        CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
        mdesc->SetMolinfo().SetBiomol(biomol);
        entry->SetSeq().SetDescr().Set().push_back(mdesc);
    }
}


void SetTech (CRef<objects::CSeq_entry> entry, objects::CMolInfo::TTech tech)
{
    bool found = false;

    NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetTech(tech);
            found = true;
        }
    }
    if (!found) {
        CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
        mdesc->SetMolinfo().SetTech(tech);
        entry->SetSeq().SetDescr().Set().push_back(mdesc);
    }
}


void SetCompleteness(CRef<objects::CSeq_entry> entry, objects::CMolInfo::TCompleteness completeness)
{
    if (entry->IsSeq()) {
        bool found = false;
        NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsMolinfo()) {
                (*it)->SetMolinfo().SetCompleteness (completeness);
                found = true;
            }
        }
        if (!found) {
            CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
            if (entry->GetSeq().IsAa()) {
                mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_peptide);
            } else {
                mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_genomic);
            }
            mdesc->SetMolinfo().SetCompleteness (completeness);
            entry->SetSeq().SetDescr().Set().push_back(mdesc);
        }
    }
}


CRef<objects::CSeq_entry> BuildGoodProtSeq(void)
{
    CRef<objects::CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PRKTEIN");
    entry->SetSeq().SetInst().SetLength(7);
    NON_CONST_ITERATE (objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_peptide);
        }
    }

    AddProtFeat (entry);

    return entry;
}


CRef<objects::CSeq_entry> MakeProteinForGoodNucProtSet (string id)
{
    // make protein
    CRef<objects::CBioseq> pseq(new objects::CBioseq());
    pseq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    pseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    pseq->SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIN");
    pseq->SetInst().SetLength(8);

    CRef<objects::CSeq_id> pid(new objects::CSeq_id());
    pid->SetLocal().SetStr (id);
    pseq->SetId().push_back(pid);

    CRef<objects::CSeqdesc> mpdesc(new objects::CSeqdesc());
    mpdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_peptide); 
    mpdesc->SetMolinfo().SetCompleteness(objects::CMolInfo::eCompleteness_complete);
    pseq->SetDescr().Set().push_back(mpdesc);

    CRef<objects::CSeq_entry> pentry(new objects::CSeq_entry());
    pentry->SetSeq(*pseq);

    CRef<objects::CSeq_feat> feat (new objects::CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr(id);
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(7);
    AddFeat (feat, pentry);

    return pentry;
}


CRef<objects::CSeq_feat> MakeCDSForGoodNucProtSet (const string& nuc_id, const string& prot_id)
{
    CRef<objects::CSeq_feat> cds (new objects::CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr(prot_id);
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr(nuc_id);
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(26);
    return cds;
}


CRef<objects::CSeq_entry> BuildGoodNucProtSet(void)
{
    CRef<objects::CBioseq_set> set(new objects::CBioseq_set());
    set->SetClass(objects::CBioseq_set::eClass_nuc_prot);

    // make nucleotide
    CRef<objects::CBioseq> nseq(new objects::CBioseq());
    nseq->SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    nseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    nseq->SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    nseq->SetInst().SetLength(60);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr ("nuc");
    nseq->SetId().push_back(id);

    CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_genomic);    
    nseq->SetDescr().Set().push_back(mdesc);

    CRef<objects::CSeq_entry> nentry(new objects::CSeq_entry());
    nentry->SetSeq(*nseq);

    set->SetSeq_set().push_back(nentry);

    // make protein
    CRef<objects::CSeq_entry> pentry = MakeProteinForGoodNucProtSet("prot");

    set->SetSeq_set().push_back(pentry);

    CRef<objects::CSeq_entry> set_entry(new objects::CSeq_entry());
    set_entry->SetSet(*set);

    CRef<objects::CSeq_feat> cds = MakeCDSForGoodNucProtSet("nuc", "prot");
    AddFeat (cds, set_entry);

    AddGoodSource (set_entry);
    AddGoodPub(set_entry);
    return set_entry;
}


void AdjustProtFeatForNucProtSet(CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_feat> prot;
    CRef<objects::CSeq_entry> prot_seq;

    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        prot_seq = entry;
        prot = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    } else if (entry->IsSet()) {
        prot_seq = entry->SetSet().SetSeq_set().back();
        prot = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    }
    if (prot && prot_seq) {
        prot->SetLocation().SetInt().SetTo(prot_seq->SetSeq().SetInst().SetLength() - 1);
    }
}


void SetNucProtSetProductName (CRef<objects::CSeq_entry> entry, string new_name)
{
    CRef<objects::CSeq_feat> prot;
    CRef<objects::CSeq_entry> prot_seq;

    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        prot_seq = entry;
        prot = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    } else if (entry->IsSet()) {
        prot_seq = entry->SetSet().SetSeq_set().back();
        prot = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    }
    if (prot) {
        if (prot->SetData().SetProt().SetName().size() > 0) {
            prot->SetData().SetProt().SetName().pop_front();
        }
        prot->SetData().SetProt().SetName().push_front(new_name);
    }
}


CRef<objects::CSeq_feat> GetCDSFromGoodNucProtSet (CRef<objects::CSeq_entry> entry)
{
    return entry->SetSet().SetAnnot().front()->SetData().SetFtable().front();
}


CRef<objects::CSeq_entry> GetNucleotideSequenceFromGoodNucProtSet (CRef<objects::CSeq_entry> entry)
{
    return entry->SetSet().SetSeq_set().front();
}


CRef<objects::CSeq_entry> GetProteinSequenceFromGoodNucProtSet (CRef<objects::CSeq_entry> entry)
{
    return entry->SetSet().SetSeq_set().back();
}


CRef<objects::CSeq_feat> GetProtFeatFromGoodNucProtSet (CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_entry> pentry = GetProteinSequenceFromGoodNucProtSet(entry);
    return pentry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
}


void RetranslateCdsForNucProtSet (CRef<objects::CSeq_entry> entry, objects::CScope &scope)
{
    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    CRef<objects::CBioseq> bioseq = CSeqTranslator::TranslateToProtein(*cds, scope);
    CRef<objects::CSeq_entry> pentry = GetProteinSequenceFromGoodNucProtSet(entry);
    pentry->SetSeq().SetInst().Assign(bioseq->GetInst());
    AdjustProtFeatForNucProtSet (entry);
}


void SetProteinPartial(CRef<CSeq_entry> pentry, bool partial5, bool partial3)
{
    CRef<CSeq_feat> prot = pentry->SetAnnot().front()->SetData().SetFtable().front();
    prot->SetPartial(partial5 || partial3);
    prot->SetLocation().SetPartialStart(partial5, objects::eExtreme_Biological);
    prot->SetLocation().SetPartialStop(partial3, objects::eExtreme_Biological);

    // molinfo completeness
    if (partial5 && partial3) {
        SetCompleteness (pentry, objects::CMolInfo::eCompleteness_no_ends);
    } else if (partial5) {
        SetCompleteness (pentry, objects::CMolInfo::eCompleteness_no_left);
    } else if (partial3) {
        SetCompleteness (pentry, objects::CMolInfo::eCompleteness_no_right);
    } else {
        SetCompleteness (pentry, objects::CMolInfo::eCompleteness_complete);
    }
}


void SetNucProtSetPartials (CRef<objects::CSeq_entry> entry, bool partial5, bool partial3)
{
    // partials for CDS
    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetPartial(partial5 || partial3);
    cds->SetLocation().SetPartialStart(partial5, objects::eExtreme_Biological);
    cds->SetLocation().SetPartialStop(partial3, objects::eExtreme_Biological);

    CRef<objects::CSeq_entry> pentry = GetProteinSequenceFromGoodNucProtSet(entry);
    SetProteinPartial(pentry, partial5, partial3);
}


void ChangeNucProtSetProteinId (CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_entry> pseq = GetProteinSequenceFromGoodNucProtSet(entry);
    pseq->SetSeq().SetId().front()->Assign(*id);

    CRef<objects::CSeq_feat> pfeat = GetProtFeatFromGoodNucProtSet(entry);
    pfeat->SetLocation().SetInt().SetId().Assign(*id);

    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetProduct().SetWhole().Assign(*id);
}


void ChangeNucProtSetNucId (CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_entry> nseq = GetNucleotideSequenceFromGoodNucProtSet(entry);
    nseq->SetSeq().SetId().front()->Assign(*id);

    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    if(cds->GetLocation().IsInt()) {
        cds->SetLocation().SetInt().SetId().Assign(*id);
    } else if (cds->GetLocation().IsMix()) {
        cds->SetLocation().SetMix().Set().front()->SetInt().SetId().Assign(*id);
        cds->SetLocation().SetMix().Set().back()->SetInt().SetId().Assign(*id);
    }
}


void MakeNucProtSet3Partial (CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(59);
    cds->SetLocation().SetPartialStop(true, objects::eExtreme_Biological);
    cds->SetPartial(true);
    CRef<objects::CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACAAAGGGATGCCCAGAAAAACAGAGATAAACAAAGGG");
    CRef<objects::CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEINKGMPRKTEINKG");
    prot_seq->SetSeq().SetInst().SetLength(20);
    SetCompleteness (prot_seq, objects::CMolInfo::eCompleteness_no_right);
    CRef<objects::CSeq_feat> prot = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot->SetLocation().SetInt().SetTo(19);
    prot->SetLocation().SetPartialStop(true, objects::eExtreme_Biological);
    prot->SetPartial(true);
    
}


void ChangeId(CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_id> id)
{
    if (annot && annot->IsFtable()) {
        objects::CSeq_annot::C_Data::TFtable::iterator it = annot->SetData().SetFtable().begin();
        while (it != annot->SetData().SetFtable().end()) {
            (*it)->SetLocation().SetInt().SetId().Assign(*id);
            ++it;
        }
    }
}


void ChangeProductId(CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_id> id)
{
    if (annot && annot->IsFtable()) {
        objects::CSeq_annot::C_Data::TFtable::iterator it = annot->SetData().SetFtable().begin();
        while (it != annot->SetData().SetFtable().end()) {
            if ((*it)->IsSetProduct()) {
                (*it)->SetProduct().SetWhole().Assign(*id);
            }
            ++it;
        }
    }
}


void ChangeNucId(CRef<objects::CSeq_entry> np_set, CRef<objects::CSeq_id> id)
{
    if (!np_set || !np_set->IsSet()) {
        return;
    }

    CRef<objects::CSeq_entry> nuc_entry = np_set->SetSet().SetSeq_set().front();

    nuc_entry->SetSeq().SetId().front()->Assign(*id);
    if (nuc_entry->SetSeq().IsSetAnnot()) {
        NON_CONST_ITERATE(objects::CSeq_entry::TAnnot, annot_it, nuc_entry->SetSeq().SetAnnot()) {
            ChangeId (*annot_it, id);
        }
    }
    if (np_set->SetSet().IsSetAnnot()) {
        NON_CONST_ITERATE(objects::CSeq_entry::TAnnot, annot_it, np_set->SetSet().SetAnnot()) {
            ChangeId (*annot_it, id);
        }
    }
}


void ChangeProtId(CRef<objects::CSeq_entry> np_set, CRef<objects::CSeq_id> id)
{
    if (!np_set || !np_set->IsSet()) {
        return;
    }

    CRef<objects::CSeq_entry> prot_entry = np_set->SetSet().SetSeq_set().back();
    CRef<objects::CSeq_feat> cds = GetCDSFromGoodNucProtSet(np_set);

    prot_entry->SetSeq().SetId().front()->Assign(*id);
    EDIT_EACH_SEQANNOT_ON_BIOSEQ (annot_it, prot_entry->SetSeq()) {
        ChangeId (*annot_it, id);
    }

    EDIT_EACH_SEQANNOT_ON_SEQSET (annot_it, np_set->SetSet()) {
        ChangeProductId (*annot_it, id);
    }
}


CRef<objects::CSeq_id> BuildRefSeqId(void)
{
    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetOther().SetAccession("NC_123456");
    return id;
}


void ChangeId(CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id)
{
    if (entry->IsSeq()) {
        entry->SetSeq().SetId().front()->Assign(*id);
        if (entry->SetSeq().IsSetAnnot()) {
            objects::CBioseq::TAnnot::iterator annot_it = entry->SetSeq().SetAnnot().begin();
            while (annot_it != entry->SetSeq().SetAnnot().end()) {
                if ((*annot_it)->IsFtable()) {
                    objects::CSeq_annot::C_Data::TFtable::iterator it = (*annot_it)->SetData().SetFtable().begin();
                    while (it != (*annot_it)->SetData().SetFtable().end()) {
                        (*it)->SetLocation().SetId(*id);
                        ++it;
                    }
                }
                ++annot_it;
            }
        }
    }
}


void ChangeId(CRef<objects::CSeq_annot> annot, string suffix)
{
    if (annot && annot->IsFtable()) {
        objects::CSeq_annot::C_Data::TFtable::iterator it = annot->SetData().SetFtable().begin();
        while (it != annot->SetData().SetFtable().end()) {
            (*it)->SetLocation().SetInt().SetId().SetLocal().SetStr().append(suffix);
            if ((*it)->IsSetProduct()) {
                (*it)->SetProduct().SetWhole().SetLocal().SetStr().append(suffix);
            }
            ++it;
        }
    }
}


void ChangeId(CRef<objects::CSeq_entry> entry, string suffix)
{
    if (entry->IsSeq()) {
        entry->SetSeq().SetId().front()->SetLocal().SetStr().append(suffix);
        if (entry->SetSeq().IsSetAnnot()) {
            objects::CBioseq::TAnnot::iterator annot_it = entry->SetSeq().SetAnnot().begin();
            while (annot_it != entry->SetSeq().SetAnnot().end()) {
                ChangeId(*annot_it, suffix);
                ++annot_it;
            }
        }
    } else if (entry->IsSet()) {
        objects::CBioseq_set::TSeq_set::iterator it = entry->SetSet().SetSeq_set().begin();
        while (it != entry->SetSet().SetSeq_set().end()) {
            ChangeId(*it, suffix);
            ++it;
        }
        if (entry->SetSet().IsSetAnnot()) {
            objects::CBioseq_set::TAnnot::iterator annot_it = entry->SetSet().SetAnnot().begin();
            while (annot_it != entry->SetSet().SetAnnot().end()) {
                ChangeId(*annot_it, suffix);
                ++annot_it;
            }
        }
    }
}


CRef<objects::CSeq_entry> BuildGenProdSetNucProtSet (CRef<objects::CSeq_id> nuc_id, CRef<objects::CSeq_id> prot_id)
{
    CRef<objects::CSeq_entry> np = BuildGoodNucProtSet();
    CRef<objects::CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(np);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAA");
    nuc->SetSeq().SetInst().SetLength(27);
    nuc->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_rna);
    SetBiomol(nuc, objects::CMolInfo::eBiomol_mRNA);
    if (nuc_id) {
        ChangeNucProtSetNucId(np, nuc_id);
    }
    if (prot_id) {
        ChangeNucProtSetProteinId(np, prot_id);
    }
    return np;
}


CRef<objects::CSeq_feat> MakemRNAForCDS (CRef<objects::CSeq_feat> feat)
{
    CRef<objects::CSeq_feat> mrna(new objects::CSeq_feat);
    mrna->SetData().SetRna().SetType(objects::CRNA_ref::eType_mRNA);
    mrna->SetLocation().Assign(feat->GetLocation());
    return mrna;
}


CRef<objects::CSeq_entry> BuildGoodGenProdSet()
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSet().SetClass(objects::CBioseq_set::eClass_gen_prod_set);
    CRef<objects::CSeq_entry> contig = BuildGoodSeq();
    contig->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    contig->SetSeq().SetInst().SetLength(60);
    entry->SetSet().SetSeq_set().push_back (contig);
    CRef<objects::CSeq_id> nuc_id(new objects::CSeq_id());
    nuc_id->SetLocal().SetStr("nuc");
    CRef<objects::CSeq_id> prot_id(new objects::CSeq_id());
    prot_id->SetLocal().SetStr("prot");
    CRef<objects::CSeq_entry> np = BuildGenProdSetNucProtSet(nuc_id, prot_id);
    entry->SetSet().SetSeq_set().push_back (np);

    CRef<objects::CSeq_feat> cds(new objects::CSeq_feat());
    cds->Assign (*(GetCDSFromGoodNucProtSet(np)));
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    AddFeat (cds, contig);
    CRef<objects::CSeq_feat> mrna = MakemRNAForCDS(cds);
    mrna->SetProduct().SetWhole().Assign(*nuc_id);
    AddFeat (mrna, contig);

    return entry;
}


CRef<objects::CSeq_entry> GetGenomicFromGenProdSet (CRef<objects::CSeq_entry> entry)
{
    return entry->SetSet().SetSeq_set().front();
}


CRef<objects::CSeq_feat> GetmRNAFromGenProdSet(CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_entry> genomic = GetGenomicFromGenProdSet(entry);
    CRef<objects::CSeq_feat> mrna = genomic->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    return mrna;
}


CRef<objects::CSeq_entry> GetNucProtSetFromGenProdSet(CRef<objects::CSeq_entry> entry)
{
    return entry->SetSet().SetSeq_set().back();
}


CRef<objects::CSeq_feat> GetCDSFromGenProdSet (CRef<objects::CSeq_entry> entry)
{
    CRef<objects::CSeq_entry> genomic = GetGenomicFromGenProdSet(entry);
    CRef<objects::CSeq_feat> cds = genomic->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    return cds;
}


void RevComp (objects::CBioseq& bioseq)
{
    if (!bioseq.IsNa() || !bioseq.IsSetInst()
        || !bioseq.GetInst().IsSetSeq_data()
        || !bioseq.GetInst().GetSeq_data().IsIupacna()) {
        return;
    }
    string seq = bioseq.GetInst().GetSeq_data().GetIupacna().Get();
    string new_seq = "";
    string::iterator sit = seq.end();
    while (sit != seq.begin()) {
        --sit;
        string new_ch = "";
        new_ch += *sit;
        if (NStr::Equal(new_ch, "A")) {
            new_ch = "T";
        } else if (NStr::Equal(new_ch, "T")) {
            new_ch = "A";
        } else if (NStr::Equal(new_ch, "G")) {
            new_ch = "C";
        } else if (NStr::Equal(new_ch, "C")) {
            new_ch = "G";
        }
        new_seq.append(new_ch);
    }

    bioseq.SetInst().SetSeq_data().SetIupacna().Set(new_seq);
    size_t len = bioseq.GetLength();
    if (bioseq.IsSetAnnot()) {
        EDIT_EACH_SEQFEAT_ON_SEQANNOT (feat_it, *(bioseq.SetAnnot().front())) {
            TSeqPos new_from = len - (*feat_it)->GetLocation().GetInt().GetTo() - 1;
            TSeqPos new_to = len - (*feat_it)->GetLocation().GetInt().GetFrom() - 1;
            (*feat_it)->SetLocation().SetInt().SetFrom(new_from);
            (*feat_it)->SetLocation().SetInt().SetTo(new_to);
            if ((*feat_it)->GetLocation().GetInt().IsSetStrand()
                && (*feat_it)->GetLocation().GetInt().GetStrand() == objects::eNa_strand_minus) {
                (*feat_it)->SetLocation().SetInt().SetStrand(objects::eNa_strand_plus);
            } else {
                (*feat_it)->SetLocation().SetInt().SetStrand(objects::eNa_strand_minus);
            }
        }
    }
}


void RevComp (objects::CSeq_loc& loc, size_t len)
{
    if (loc.IsInt()) {
        TSeqPos new_from = len - loc.GetInt().GetTo() - 1;
        TSeqPos new_to = len - loc.GetInt().GetFrom() - 1;
        loc.SetInt().SetFrom(new_from);
        loc.SetInt().SetTo(new_to);
        if (loc.GetInt().IsSetStrand()
            && loc.GetInt().GetStrand() == eNa_strand_minus) {
            loc.SetInt().SetStrand(eNa_strand_plus);
        } else {
            loc.SetInt().SetStrand(eNa_strand_minus);
        }
    } else if (loc.IsMix()) {
        NON_CONST_ITERATE (objects::CSeq_loc_mix::Tdata, it, loc.SetMix().Set()) {
            RevComp (**it, len);
        }
    }
}


void RevComp (CRef<objects::CSeq_entry> entry)
{
    if (entry->IsSeq()) {
        RevComp(entry->SetSeq());
    } else if (entry->IsSet()) {
        if (entry->GetSet().IsSetClass()
            && entry->GetSet().GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
            RevComp(entry->SetSet().SetSeq_set().front());
            size_t len = entry->GetSet().GetSeq_set().front()->GetSeq().GetLength();
            EDIT_EACH_SEQFEAT_ON_SEQANNOT (feat_it, *(entry->SetSet().SetAnnot().front())) {
                RevComp ((*feat_it)->SetLocation(), len);
            }
        }
    }
}


CRef<objects::CSeq_entry> BuildGoodDeltaSeq(void)
{
    CRef<objects::CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATG", objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(34);

    return entry;
}


void RemoveDeltaSeqGaps(CRef<objects::CSeq_entry> entry) 
{
    objects::CDelta_ext::Tdata::iterator seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin();
    while (seg_it != entry->SetSeq().SetInst().SetExt().SetDelta().Set().end()) {
        if ((*seg_it)->IsLiteral() 
            && (!(*seg_it)->GetLiteral().IsSetSeq_data() 
                || (*seg_it)->GetLiteral().GetSeq_data().IsGap())) {
            TSeqPos len = entry->SetSeq().SetInst().GetLength();
            len -= (*seg_it)->GetLiteral().GetLength();
            seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().erase(seg_it);
            entry->SetSeq().SetInst().SetLength(len);
        } else {
            ++seg_it;
        }
    }
}


void AddToDeltaSeq(CRef<objects::CSeq_entry> entry, string seq)
{
    size_t orig_len = entry->GetSeq().GetLength();
    size_t add_len = seq.length();

    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(seq, objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(orig_len + 10 + add_len);
}


CRef<objects::CSeq_entry> BuildSegSetPart(string id_str)
{
    CRef<objects::CSeq_entry> part(new objects::CSeq_entry());
    part->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    part->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    part->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    part->SetSeq().SetInst().SetLength(60);
    CRef<objects::CSeq_id> id(new objects::CSeq_id(id_str));
    part->SetSeq().SetId().push_back(id);
    SetBiomol(part, objects::CMolInfo::eBiomol_genomic);
    return part;
}


CRef<objects::CSeq_entry> BuildGoodSegSet(void)
{
    CRef<objects::CSeq_entry> segset(new objects::CSeq_entry());
    segset->SetSet().SetClass(objects::CBioseq_set::eClass_segset);

    CRef<objects::CSeq_entry> seg_seq(new objects::CSeq_entry());
    seg_seq->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    seg_seq->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_seg);

    CRef<objects::CSeq_loc> loc1(new objects::CSeq_loc());
    loc1->SetWhole().SetLocal().SetStr("part1");
    CRef<objects::CSeq_loc> loc2(new objects::CSeq_loc());
    loc2->SetWhole().SetLocal().SetStr("part2");
    CRef<objects::CSeq_loc> loc3(new objects::CSeq_loc());
    loc3->SetWhole().SetLocal().SetStr("part3");

    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    seg_seq->SetSeq().SetInst().SetLength(180);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr ("master");
    seg_seq->SetSeq().SetId().push_back(id);
    seg_seq->SetSeq().SetInst().SetLength(180);
    SetBiomol(seg_seq, objects::CMolInfo::eBiomol_genomic);

    segset->SetSet().SetSeq_set().push_back(seg_seq);

    // create parts set
    CRef<objects::CSeq_entry> parts_set(new objects::CSeq_entry());
    parts_set->SetSet().SetClass(objects::CBioseq_set::eClass_parts);
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part1"));
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part2"));
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part3"));

    segset->SetSet().SetSeq_set().push_back(parts_set);

//    CRef<objects::CSeqdesc> pdesc(new objects::CSeqdesc());
//    CRef<objects::CPub> pub(new objects::CPub());
//    pub->SetPmid((objects::CPub::TPmid)1);
//    pdesc->SetPub().SetPub().Set().push_back(pub);
//    segset->SetDescr().Set().push_back(pdesc);
    AddGoodPub(segset);
    CRef<objects::CSeqdesc> odesc(new objects::CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname("Sebaea microphylla");
    odesc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    CRef<objects::CDbtag> taxon_id(new objects::CDbtag());
    taxon_id->SetDb("taxon");
    taxon_id->SetTag().SetId(592768);
    odesc->SetSource().SetOrg().SetDb().push_back(taxon_id);
    CRef<objects::CSubSource> subsrc(new objects::CSubSource());
    subsrc->SetSubtype(objects::CSubSource::eSubtype_chromosome);
    subsrc->SetName("1");
    odesc->SetSource().SetSubtype().push_back(subsrc);
    segset->SetDescr().Set().push_back(odesc);

    return segset;
}


CRef<objects::CSeq_entry> BuildGoodEcoSet()
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSet().SetClass(objects::CBioseq_set::eClass_eco_set);
    CRef<objects::CSeq_entry> seq1 = BuildGoodSeq();
    ChangeId(seq1, "1");
    CRef<objects::CSeq_entry> seq2 = BuildGoodSeq();
    ChangeId(seq2, "2");
    CRef<objects::CSeq_entry> seq3 = BuildGoodSeq();
    ChangeId(seq3, "3");
    entry->SetSet().SetSeq_set().push_back(seq1);
    entry->SetSet().SetSeq_set().push_back(seq2);
    entry->SetSet().SetSeq_set().push_back(seq3);

    CRef<objects::CSeqdesc> desc(new objects::CSeqdesc());
    desc->SetTitle("popset title");
    entry->SetSet().SetDescr().Set().push_back(desc);

    return entry;
}


CRef<objects::CSeq_entry> BuildGoodEcoSetWithAlign(size_t front_insert)
{
    CRef<CSeq_entry> entry = BuildGoodEcoSet();

    CRef<objects::CSeq_align> align(new CSeq_align());
    align->SetType(objects::CSeq_align::eType_global);
    align->SetDim(entry->GetSet().GetSeq_set().size());
    size_t offset = 0;
    for (auto& s : entry->SetSet().SetSeq_set()) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(s->GetSeq().GetId().front()));
        align->SetSegs().SetDenseg().SetIds().push_back(id);
        if (offset > 0) {
            const string& orig = s->SetSeq().SetInst().SetSeq_data().SetIupacna().Set();
            size_t orig_len = s->GetSeq().GetInst().GetLength();
            string add = "";
            for (auto i = (size_t)0; i < offset; i++) {
                add += "A";
            }
            s->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(add + orig);
            s->SetSeq().SetInst().SetLength(orig_len + offset);
        }
        align->SetSegs().SetDenseg().SetStarts().push_back(offset);
        offset += front_insert;
    }
    align->SetSegs().SetDenseg().SetNumseg(1);
    align->SetSegs().SetDenseg().SetLens().push_back(entry->GetSet().GetSeq_set().front()->GetSeq().GetInst().GetLength());
    align->SetSegs().SetDenseg().SetDim(3);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);
    return entry;
}


// assumes that sequence has been reverse-complemented
void ReverseAlignmentStrand(CDense_seg& denseg, size_t pos, size_t seq_len)
{
    // prepopulate the strand array if not already present
    auto num_pieces = denseg.GetDim() * denseg.GetNumseg();
    if (!denseg.IsSetStrands()) {
        for (auto i = 0; i < num_pieces; i++) {
            denseg.SetStrands().push_back(eNa_strand_plus);
        }
    } else if (denseg.GetStrands().size() < num_pieces) {
        for (auto i = denseg.GetStrands().size(); i < num_pieces; i++) {
            denseg.SetStrands().push_back(eNa_strand_plus);
        }
    }
    for (auto i = 0; i < denseg.GetNumseg(); i++) {
        auto offset = i * denseg.GetDim() + pos;
        auto orig = denseg.GetStarts()[offset];
        if (orig > -1) {
            denseg.SetStarts()[offset] = seq_len - orig - denseg.GetLens()[i];
        }
        if (denseg.GetStrands()[offset] == eNa_strand_minus) {
            denseg.SetStrands()[offset] = eNa_strand_plus;
        } else {
            denseg.SetStrands()[offset] = eNa_strand_minus;
        }
    }
}


CRef<objects::CSeq_align> BuildGoodAlign()
{
    CRef<objects::CSeq_align> align(new objects::CSeq_align());
    CRef<objects::CSeq_id> id1(new objects::CSeq_id());
    id1->SetGenbank().SetAccession("FJ375734.2");
    id1->SetGenbank().SetVersion(2);
    CRef<objects::CSeq_id> id2(new objects::CSeq_id());
    id2->SetGenbank().SetAccession("FJ375735.2");
    id2->SetGenbank().SetVersion(2);
    align->SetDim(2);
    align->SetType(objects::CSeq_align::eType_global);
    align->SetSegs().SetDenseg().SetIds().push_back(id1);
    align->SetSegs().SetDenseg().SetIds().push_back(id2);
    align->SetSegs().SetDenseg().SetDim(2);
    align->SetSegs().SetDenseg().SetStarts().push_back(0);
    align->SetSegs().SetDenseg().SetStarts().push_back(0);
    align->SetSegs().SetDenseg().SetNumseg(1);
    align->SetSegs().SetDenseg().SetLens().push_back(812);

    return align;
}


CRef<objects::CSeq_annot> BuildGoodGraphAnnot(string id)
{
    CRef<objects::CSeq_graph> graph(new objects::CSeq_graph());
    graph->SetLoc().SetInt().SetFrom(0);
    graph->SetLoc().SetInt().SetTo(10);
    graph->SetLoc().SetInt().SetId().SetLocal().SetStr(id);

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot());
    annot->SetData().SetGraph().push_back(graph);

    return annot;
}


void RemoveDescriptorType (CRef<objects::CSeq_entry> entry, objects::CSeqdesc::E_Choice desc_choice)
{
    EDIT_EACH_DESCRIPTOR_ON_SEQENTRY (dit, *entry) {
        if ((*dit)->Which() == desc_choice) {
            ERASE_DESCRIPTOR_ON_SEQENTRY (dit, *entry);
        }
    }
}


CRef<objects::CSeq_feat> BuildtRNA(CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat());
    feat->SetLocation().SetInt().SetId().Assign(*id);
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(10);

    feat->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    feat->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('N');
    feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetId().Assign(*id);
    feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(11);
    feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(13);

    return feat;
}


CRef<objects::CSeq_feat> BuildGoodtRNA(CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_feat> trna = BuildtRNA(id);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(8);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('F');
    return trna;
}


CRef<objects::CSeq_loc> MakeMixLoc (CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_loc> loc1(new objects::CSeq_loc());
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(15);
    loc1->SetInt().SetId().Assign(*id);
    CRef<objects::CSeq_loc> loc2(new objects::CSeq_loc());
    loc2->SetInt().SetFrom(46);
    loc2->SetInt().SetTo(56);
    loc2->SetInt().SetId().Assign(*id);
    CRef<objects::CSeq_loc> mixloc(new objects::CSeq_loc());
    mixloc->SetMix().Set().push_back(loc1);
    mixloc->SetMix().Set().push_back(loc2);
    return mixloc;
}


CRef<objects::CSeq_feat> MakeIntronForMixLoc (CRef<objects::CSeq_id> id)
{
    CRef<objects::CSeq_feat> intron (new objects::CSeq_feat());
    intron->SetData().SetImp().SetKey("intron");
    intron->SetLocation().SetInt().SetFrom(16);
    intron->SetLocation().SetInt().SetTo(45);
    intron->SetLocation().SetInt().SetId().Assign(*id);
    return intron;
}


void SetSpliceForMixLoc (objects::CBioseq& seq)
{
    seq.SetInst().SetSeq_data().SetIupacna().Set()[16] = 'G';
    seq.SetInst().SetSeq_data().SetIupacna().Set()[17] = 'T';
    seq.SetInst().SetSeq_data().SetIupacna().Set()[44] = 'A';
    seq.SetInst().SetSeq_data().SetIupacna().Set()[45] = 'G';
}


CRef<objects::CSeq_feat> MakeGeneForFeature (CRef<objects::CSeq_feat> feat)
{
    CRef<objects::CSeq_feat> gene(new objects::CSeq_feat());
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetLocation().SetInt().SetId().Assign(*(feat->GetLocation().GetId()));
    gene->SetLocation().SetInt().SetStrand(feat->GetLocation().GetStrand());
    gene->SetLocation().SetInt().SetFrom(feat->GetLocation().GetStart(objects::eExtreme_Positional));
    gene->SetLocation().SetInt().SetTo(feat->GetLocation().GetStop(objects::eExtreme_Positional));
    gene->SetLocation().SetPartialStart(feat->GetLocation().IsPartialStart(objects::eExtreme_Positional), objects::eExtreme_Positional);
    gene->SetLocation().SetPartialStop(feat->GetLocation().IsPartialStop(objects::eExtreme_Positional), objects::eExtreme_Positional);
    if (feat->IsSetPartial() && feat->GetPartial()) {
        gene->SetPartial(true);
    }
    return gene;
}


CRef<objects::CSeq_feat> AddGoodImpFeat (CRef<objects::CSeq_entry> entry, string key)
{
    CRef<objects::CSeq_feat> imp_feat = AddMiscFeature (entry, 10);
    imp_feat->SetData().SetImp().SetKey(key);
    if (NStr::Equal(key, "conflict")) {
        imp_feat->AddQualifier("citation", "1");
    } else if (NStr::Equal(key, "intron")) {
        entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[0] = 'G';
        entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[1] = 'T';
        entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[9] = 'A';
        entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[10] = 'G';
    } else if (NStr::Equal(key, "misc_binding") || NStr::Equal(key, "protein_bind")) {
        imp_feat->AddQualifier("bound_moiety", "foo");
    } else if (NStr::Equal(key, "modified_base")) {
        imp_feat->AddQualifier("mod_base", "foo");
    } else if (NStr::Equal(key, "old_sequence")) {
        imp_feat->AddQualifier("citation", "1");
    } else if (NStr::Equal(key, "operon")) {
        imp_feat->AddQualifier("operon", "foo");
    } else if (NStr::Equal(key, "polyA_site")) {
        imp_feat->SetLocation().SetPnt().SetId().SetLocal().SetStr("good");
        imp_feat->SetLocation().SetPnt().SetPoint(5);
    } else if (NStr::Equal(key, "source")) {
        imp_feat->AddQualifier("organism", "foo");
    } 
    return imp_feat;
}

// helper classes for TraverseAndRunTestCases
namespace {

    // This just accumulates all the files in the path
    struct SFileRememberer 
    {
        void operator()( const CDirEntry & dir_entry ) {
            m_filesFound.push_back(CFile(dir_entry));
        }

        vector<CFile> m_filesFound;
    };

    // a simple function object that extracts the
    // first of a pair. (Unfortunately, "select1st" is not part of
    // the STL standard so it can't be relied upon to exist)
    template<typename Pair>
    struct SFirstOfPair
    {
        typename Pair::first_type operator()( const Pair & a_pair ) const
        {
            return a_pair.first;
        }
    };
}

void TraverseAndRunTestCases(
    ITestRunner *pTestRunner,
    CDir dirWithTestCases,
    const set<string> & setOfRequiredSuffixes,
    const set<string> & setOfOptionalSuffixes,
    const set<string> & setOfIgnoredSuffixes,
    TTraverseAndRunTestCasesFlags fFlags )
{
    if( ! pTestRunner ) {
        NCBI_USER_THROW_FMT("NULL pTestRunner");
    }
    if( ! dirWithTestCases.Exists() ) {
        pTestRunner->OnError("Top-level test-cases dir not found: " + dirWithTestCases.GetPath() );
        return;
    }
    if( ! dirWithTestCases.IsDir() ) {
        pTestRunner->OnError("Top-level test-cases dir is actually not a dir: " + dirWithTestCases.GetPath() );
        return;
    }

    const vector<string> kEmptyVectorOfStrings;

    SFileRememberer fileRememberer;
    FindFilesInDir(
        dirWithTestCases,
        kEmptyVectorOfStrings,
        kEmptyVectorOfStrings,
        fileRememberer,
        fFF_File | fFF_Recursive );

    // this is what we search for to see if there is a hidden directory
    // or file anywhere along the path.
    const string kHiddenSubstring = CDirEntry::GetPathSeparator() + string(".svn") + CDirEntry::GetPathSeparator();

    typedef map<string, ITestRunner::TMapSuffixToFile> TMapTestNameToItsFiles;
    TMapTestNameToItsFiles mapTestNameToItsFiles;
    // this loop loads mapTestNameToItsFiles
    ITERATE( vector<CFile>, file_it, fileRememberer.m_filesFound ) {
        const string sFileName = file_it->GetName();
        const string sFileAbsPath = CDirEntry::CreateAbsolutePath(file_it->GetPath());
        
        // hidden folders or files of any kind are silently ignored
        if( NStr::Find(sFileAbsPath, kHiddenSubstring) != NPOS ) {
            continue;
        }

        if( ! (fFlags & fTraverseAndRunTestCasesFlags_DoNOTIgnoreREADMEFiles) &&
            NStr::StartsWith(sFileName, "README") ) 
        {
            // if requested, silently ignore files starting with README
            continue;
        }

        // extract out testname and suffix
        string sTestName;
        string sSuffix;
        NStr::SplitInTwo(sFileName, ".", sTestName, sSuffix);
        if( sTestName.empty() || sSuffix.empty() ) {
            pTestRunner->OnError("Bad file name: " + file_it->GetPath());
            continue;
        }

        if( setOfIgnoredSuffixes.find(sSuffix) != setOfIgnoredSuffixes.end() ) {
            // silently ignores suffixes requested to be ignored by the user
            continue;
        }

        // load this entry, with error if not inserted
        const bool bWasInserted =
            mapTestNameToItsFiles[sTestName].insert(make_pair(sSuffix, *file_it)).second;
        if( ! bWasInserted ) {
            pTestRunner->OnError( 
                "File with same name appears multiple times in different dirs: " +
                file_it->GetPath() );
            continue;
        }
    }

    // sanity check all tests and remove the unusable ones
    ERASE_ITERATE(TMapTestNameToItsFiles, test_it, mapTestNameToItsFiles) {
        const string & sTestName = test_it->first;
        const ITestRunner::TMapSuffixToFile & mapSuffixToFile =
            test_it->second;

        // get the keys (that is, the suffixes) of the map
        set<string> setOfAllSuffixes;
        transform(mapSuffixToFile.begin(), mapSuffixToFile.end(),
            inserter(setOfAllSuffixes, setOfAllSuffixes.begin()),
            SFirstOfPair<ITestRunner::TMapSuffixToFile::value_type>() );

        // get the non-required suffixes that were used
        set<string> setOfNonRequiredSuffixes;
        set_difference( setOfAllSuffixes.begin(), setOfAllSuffixes.end(),
            setOfRequiredSuffixes.begin(), setOfRequiredSuffixes.end(),
            inserter(setOfNonRequiredSuffixes, setOfNonRequiredSuffixes.begin() ) );
        
        // make sure it has all required suffixes
        // (the set of suffixes should have shrunk by exactly the number of required
        // suffixes on the set_difference just above)
        const size_t szNumOfSuffixes = setOfAllSuffixes.size();
        const size_t szNumOfNonRequiredSuffixes = setOfNonRequiredSuffixes.size();
        if( (szNumOfSuffixes - szNumOfNonRequiredSuffixes) != setOfRequiredSuffixes.size() ) 
        {
            pTestRunner->OnError("Skipping this test because it's missing some files: " + sTestName);
            mapTestNameToItsFiles.erase(test_it);
            continue;
        }

        // all non-required suffixes should be in the optional set
        if( ! includes( setOfOptionalSuffixes.begin(), setOfOptionalSuffixes.end(),
            setOfNonRequiredSuffixes.begin(), setOfNonRequiredSuffixes.end() ) ) 
        {
            pTestRunner->OnError("Skipping this test because it has unexpected suffix(es): " + sTestName);
            mapTestNameToItsFiles.erase(test_it);
            continue;
        }
    }

    // there should be at least one test to run
    if( mapTestNameToItsFiles.empty() ) {
        pTestRunner->OnError("There are no tests to run");
        return;
    }

    // Now, actually run the tests
    ITERATE(TMapTestNameToItsFiles, test_it, mapTestNameToItsFiles) {
        const string & sTestName = test_it->first;
        const ITestRunner::TMapSuffixToFile & mapSuffixToFile =
            test_it->second;

        cerr << "Running test: " << sTestName << endl;
        pTestRunner->RunTest(sTestName, mapSuffixToFile);
    }
}

END_SCOPE(unit_test_util)
END_SCOPE(objects)
END_NCBI_SCOPE
