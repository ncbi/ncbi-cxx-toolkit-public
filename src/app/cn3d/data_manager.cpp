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
* Authors:  Paul Thiessen
*
* File Description:
*      class to manage different root ASN data types
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/11/27 16:26:08  thiessen
* major update to data management system
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>

#include <objects/ncbimime/Entrez_general.hpp>
#include <objects/ncbimime/Biostruc_align.hpp>
#include <objects/ncbimime/Biostruc_align_seq.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/ncbimime/Biostruc_seqs.hpp>
#include <objects/ncbimime/Biostruc_seqs_aligns_cdd.hpp>
#include <objects/ncbimime/Bundle_seqs_aligns.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/cdd/Cdd_descr_set.hpp>
#include <objects/cdd/Cdd_descr.hpp>

#include "cn3d/data_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/alignment_set.hpp"
#include "cn3d/asn_reader.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

ASNDataManager::ASNDataManager(ncbi::objects::CNcbi_mime_asn1 *mime)
{
    mimeData.Reset(mime);
    Load();
}

ASNDataManager::ASNDataManager(ncbi::objects::CCdd *cdd)
{
    cddData.Reset(cdd);
    Load();
}

ASNDataManager::~ASNDataManager(void)
{
}

void ASNDataManager::Load(void)
{
    // initialization
    seqEntryList = NULL;
    masterBiostruc = NULL;
    biostrucList = NULL;
    sequenceAlignments = NULL;
    structureAlignments = NULL;
    bundleImports = NULL;
    cddUpdates = NULL;
    dataChanged = 0;

    // mime
    if (mimeData.NotEmpty()) {
        if (mimeData->IsEntrez()) {
            if (mimeData->GetEntrez().GetData().IsStructure())
                masterBiostruc = &(mimeData->SetEntrez().SetData().SetStructure());
        }

        else if (mimeData->IsAlignstruc()) {
            seqEntryList = &(mimeData->SetAlignstruc().SetSequences());
            masterBiostruc = &(mimeData->SetAlignstruc().SetMaster());
            biostrucList = &(mimeData->SetAlignstruc().SetSlaves());
            structureAlignments = &(mimeData->SetAlignstruc().SetAlignments());
            sequenceAlignments = &(mimeData->SetAlignstruc().SetSeqalign());
        }

        else if (mimeData->IsAlignseq()) {
            seqEntryList = &(mimeData->SetAlignseq().SetSequences());
            sequenceAlignments = &(mimeData->SetAlignseq().SetSeqalign());
        }

        else if (mimeData->IsStrucseq()) {
            seqEntryList = &(mimeData->SetStrucseq().SetSequences());
            masterBiostruc = &(mimeData->SetStrucseq().SetStructure());
        }

        else if (mimeData->IsStrucseqs()) {
            seqEntryList = &(mimeData->SetStrucseqs().SetSequences());
            masterBiostruc = &(mimeData->SetStrucseqs().SetStructure());
            sequenceAlignments = &(mimeData->SetStrucseqs().SetSeqalign());
        }

        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().IsSetStructures())
                biostrucList = &(mimeData->SetGeneral().SetStructures());

            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle()) {
                if (mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetSequences())
                    seqEntryList = &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetSequences());
                if (mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetStrucaligns())
                    structureAlignments =
                        &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetStrucaligns());
                if (mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetSeqaligns())
                    sequenceAlignments =
                        &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetSeqaligns());
                if (mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetImports()) {
                    bundleImports = &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetImports());
                    // make "fake" Update-aligns from imports (to pass to alignment manager)
                    SeqAnnotList::const_iterator i,
                        ie = mimeData->GetGeneral().GetSeq_align_data().GetBundle().GetImports().end();
                    for (i=mimeData->GetGeneral().GetSeq_align_data().GetBundle().GetImports().begin();
                         i!= ie; i++) {
                        CRef < CUpdate_align > update(new CUpdate_align());
                        update->SetSeqannot(i->GetPointer());
                        update->SetType(CUpdate_align::eType_other);
                        bundleImportsFaked.push_back(update);
                    }
                }
            }

            else {  // cdd
                if (mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetSequences()) {
                    if (mimeData->GetGeneral().GetSeq_align_data().GetCdd().GetSequences().IsSeq()) {
                        // convert Seq-entry type from seq to set
                        CRef < CSeq_entry > seqEntry(
                            &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetSequences()));
                        mimeData->SetGeneral().SetSeq_align_data().SetCdd().ResetSequences();
                        mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetSequences().
                            SetSet().SetSeq_set().push_back(seqEntry);
                    }
                    seqEntryList =
                        &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetSequences().SetSet().SetSeq_set());
                }
                if (mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetFeatures())
                    structureAlignments = &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetFeatures());
                if (mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetSeqannot())
                    sequenceAlignments = &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetSeqannot());
                if (mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetPending())
                    cddUpdates = &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetPending());
            }
        }

        else
            ERR_POST(Error << "Unrecognized mime type!");
    }

    // CDD
    else {
        if (cddData->IsSetSequences()) {
            if (cddData->GetSequences().IsSeq()) {
                // convert Seq-entry type from seq to set
                CRef < CSeq_entry > seqEntry(&(cddData->SetSequences()));
                cddData->ResetSequences();
                cddData->SetSequences().SetSet().SetSeq_set().push_back(seqEntry);
            }
            seqEntryList = &(cddData->SetSequences().SetSet().SetSeq_set());
        }
        if (cddData->IsSetFeatures())
            structureAlignments = &(cddData->SetFeatures());
        if (cddData->IsSetSeqannot())
            sequenceAlignments = &(cddData->SetSeqannot());
        if (cddData->IsSetPending())
            cddUpdates = &(cddData->SetPending());
    }

    // sequence list is interpreted differently for single structure
    isSingleStructure = (mimeData.NotEmpty() &&
        (mimeData->IsStrucseq() ||
         (mimeData->IsGeneral() && !structureAlignments && biostrucList && biostrucList->size() == 1)));

    // pre-screen sequence alignments to make sure they're all a type we can deal with
    if (sequenceAlignments) {
        std::list < CRef < CSeq_align > > validAlignments;
        SeqAnnotList::const_iterator n, ne = sequenceAlignments->end();
        for (n=sequenceAlignments->begin(); n!=ne; n++) {

            if (!n->GetObject().GetData().IsAlign()) {
                ERR_POST(Error << "Warning - confused by seqannot data format");
                continue;
            }
            if (n != sequenceAlignments->begin()) TESTMSG("multiple Seq-annots");

            CSeq_annot::C_Data::TAlign::const_iterator
                a, ae = n->GetObject().GetData().GetAlign().end();
            for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; a++) {

                // verify this is a type of alignment we can deal with
                if (!(a->GetObject().GetType() != CSeq_align::eType_partial ||
                    a->GetObject().GetType() != CSeq_align::eType_diags) ||
                    !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2 ||
                    (!a->GetObject().GetSegs().IsDendiag() && !a->GetObject().GetSegs().IsDenseg())) {
                    ERR_POST(Error << "Warning - confused by alignment type");
                    continue;
                }
                validAlignments.push_back(*a);
            }
        }

        sequenceAlignments->clear();
        if (validAlignments.size() == 0) {
            ERR_POST(Error << "Warning - no valid Seq-aligns present");
            sequenceAlignments = NULL;
        } else {
            sequenceAlignments->push_back(CRef < CSeq_annot > (new CSeq_annot()));
            // copy list of valid alignments only
            sequenceAlignments->front()->SetData().SetAlign() = validAlignments;
        }
    }
}

const CCn3d_style_dictionary * ASNDataManager::GetStyleDictionary(void) const
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc() && mimeData->GetAlignstruc().IsSetStyle_dictionary())
            return &(mimeData->SetAlignstruc().SetStyle_dictionary());
        else if (mimeData->IsAlignseq() && mimeData->GetAlignseq().IsSetStyle_dictionary())
            return &(mimeData->SetAlignseq().SetStyle_dictionary());
        else if (mimeData->IsStrucseq() && mimeData->GetStrucseq().IsSetStyle_dictionary())
            return &(mimeData->SetStrucseq().SetStyle_dictionary());
        else if (mimeData->IsStrucseqs() && mimeData->GetStrucseqs().IsSetStyle_dictionary())
            return &(mimeData->SetStrucseqs().SetStyle_dictionary());
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle() &&
                mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetStyle_dictionary())
                return &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetStyle_dictionary());
            else if (mimeData->GetGeneral().GetSeq_align_data().IsCdd() &&
                     mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetStyle_dictionary())
                return &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetStyle_dictionary());
        }
    } else if (cddData->IsSetStyle_dictionary())
        return &(cddData->SetStyle_dictionary());

    return NULL;
}

void ASNDataManager::SetStyleDictionary(ncbi::CRef < ncbi::objects::CCn3d_style_dictionary >& styles)
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc()) mimeData->SetAlignstruc().SetStyle_dictionary(styles);
        else if (mimeData->IsAlignseq()) mimeData->SetAlignseq().SetStyle_dictionary(styles);
        else if (mimeData->IsStrucseq()) mimeData->SetStrucseq().SetStyle_dictionary(styles);
        else if (mimeData->IsStrucseqs()) mimeData->SetStrucseqs().SetStyle_dictionary(styles);
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle())
                mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetStyle_dictionary(styles);
            else
                mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetStyle_dictionary(styles);
        }
    } else
        cddData->SetStyle_dictionary(styles);
}

const CCn3d_user_annotations * ASNDataManager::GetUserAnnotations(void) const
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc() && mimeData->GetAlignstruc().IsSetUser_annotations())
            return &(mimeData->SetAlignstruc().SetUser_annotations());
        else if (mimeData->IsAlignseq() && mimeData->GetAlignseq().IsSetUser_annotations())
            return &(mimeData->SetAlignseq().SetUser_annotations());
        else if (mimeData->IsStrucseq() && mimeData->GetStrucseq().IsSetUser_annotations())
            return &(mimeData->SetStrucseq().SetUser_annotations());
        else if (mimeData->IsStrucseqs() && mimeData->GetStrucseqs().IsSetUser_annotations())
            return &(mimeData->SetStrucseqs().SetUser_annotations());
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle() &&
                mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetUser_annotations())
                return &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetUser_annotations());
            else if (mimeData->GetGeneral().GetSeq_align_data().IsCdd() &&
                     mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetUser_annotations())
                return &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetUser_annotations());
        }
    } else if (cddData->IsSetUser_annotations())
        return &(cddData->SetUser_annotations());

    return NULL;
}

void ASNDataManager::SetUserAnnotations(ncbi::CRef < ncbi::objects::CCn3d_user_annotations >& annots)
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc()) mimeData->SetAlignstruc().SetUser_annotations(annots);
        else if (mimeData->IsAlignseq()) mimeData->SetAlignseq().SetUser_annotations(annots);
        else if (mimeData->IsStrucseq()) mimeData->SetStrucseq().SetUser_annotations(annots);
        else if (mimeData->IsStrucseqs()) mimeData->SetStrucseqs().SetUser_annotations(annots);
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle())
                mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetUser_annotations(annots);
            else
                mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetUser_annotations(annots);
        }
    } else
        cddData->SetUser_annotations(annots);
}

void ASNDataManager::SetStructureAlignments(ncbi::objects::CBiostruc_annot_set *structureAlignments)
{
    if (mimeData.NotEmpty() && mimeData->IsGeneral() && mimeData->GetGeneral().GetSeq_align_data().IsBundle())
        mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetStrucaligns(
            CRef < CBiostruc_annot_set > (structureAlignments));
    else if (cddData.NotEmpty())
        cddData->SetFeatures(CRef < CBiostruc_annot_set > (structureAlignments));
    else
        ERR_POST(Error << "ASNDataManager::AddStructureAlignments() - can't add to this data type");
}

void ASNDataManager::ReplaceUpdates(const UpdateAlignList& newUpdates)
{
    if (mimeData.NotEmpty() && mimeData->IsGeneral()) {
        if (mimeData->GetGeneral().GetSeq_align_data().IsCdd()) {
            mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetPending() = newUpdates;
            cddUpdates = &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetPending());
        } else {
            bundleImports = &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetImports());
            bundleImports->clear();
            // convert to plain imports to store in mime data (some data loss...)
            UpdateAlignList::const_iterator u, ue = newUpdates.end();
            for (u=newUpdates.begin(); u!=ue; u++)
                if ((*u)->IsSetSeqannot())
                    bundleImports->push_back(CRef < CSeq_annot > (&((*u)->SetSeqannot())));
        }
    } else if (cddData.NotEmpty()) {
        cddData->SetPending() = newUpdates;
        cddUpdates = &(cddData->SetPending());
    } else
        ERR_POST(Error << "ASNDataManager::ReplaceUpdates() - can't put updates in this data type");

    SetDataChanged(eUpdateData);
}

void ASNDataManager::RemoveUnusedSequences(const AlignmentSet *alignmentSet,
    const SequenceList& updateSequences)
{
    if (!alignmentSet) return; // don't do this for single structures

    if (!seqEntryList) {
        ERR_POST(Error << "ASNDataManager::RemoveUnusedSequences() - can't find sequence list");
        return;
    }

    // update the asn sequences, keeping only those used in the multiple alignment and updates
    seqEntryList->clear();
    std::map < const CBioseq *, bool > usedSeqs;
    int nStructuredSlaves = 0;

// macro to add the sequence to the list if not already present
#define CONDITIONAL_ADD_SEQENTRY(seq) do { \
    if (usedSeqs.find((seq)->bioseqASN.GetPointer()) == usedSeqs.end()) { \
        seqEntryList->resize(seqEntryList->size() + 1); \
        seqEntryList->back().Reset(new CSeq_entry); \
        seqEntryList->back().GetObject().SetSeq((seq)->bioseqASN); \
        usedSeqs[(seq)->bioseqASN.GetPointer()] = true; \
    } } while (0)

    // always add master first
    CONDITIONAL_ADD_SEQENTRY(alignmentSet->master);
    // add from alignmentSet slaves
    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; a++) {
        CONDITIONAL_ADD_SEQENTRY((*a)->slave);
        if ((*a)->slave->molecule) nStructuredSlaves++;
    }

    // add from updates
    SequenceList::const_iterator s, se = updateSequences.end();
    for (s=updateSequences.begin(); s!=se; s++)
        CONDITIONAL_ADD_SEQENTRY((*s));

    SetDataChanged(eSequenceData);

    // warn user if # structured slaves != # structure alignments
    if (structureAlignments && nStructuredSlaves !=
            structureAlignments->GetFeatures().front().GetObject().GetFeatures().size())
        ERR_POST(Error << "Warning: Structure alignment list does not contain one alignment per "
            "structured sequence!\nYou should recompute structure alignments before saving "
            "in order to sync the lists.");
}

bool ASNDataManager::WriteDataToFile(const char *filename, bool isBinary,
    std::string *err, ncbi::EFixNonPrint fixNonPrint) const
{
    if (mimeData.NotEmpty())
        return WriteASNToFile(filename, mimeData.GetObject(), isBinary, err, fixNonPrint);
    else
        return WriteASNToFile(filename, cddData.GetObject(), isBinary, err, fixNonPrint);
}

ncbi::objects::CCdd * ASNDataManager::GetInternalCDDData(void) const
{
    if (cddData.NotEmpty()) return cddData.GetPointer();
    else if (mimeData.NotEmpty() && mimeData->IsGeneral() &&
             mimeData->GetGeneral().GetSeq_align_data().IsCdd())
        return &(mimeData->SetGeneral().SetSeq_align_data().SetCdd());
    else
        return NULL;
}

bool ASNDataManager::IsCDD(void) const
{
    return (GetInternalCDDData() != NULL);
}

const std::string& ASNDataManager::GetCDDName(void) const
{
    static const std::string empty = "";
    CCdd *cdd = GetInternalCDDData();
    if (cdd)
        return cdd->GetName();
    else
        return empty;
}

bool ASNDataManager::SetCDDName(const std::string& name)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd || name.size() == 0) return false;
    cdd->SetName(name);
    SetDataChanged(eCDDData);
    return true;
}

const std::string& ASNDataManager::GetCDDDescription(void) const
{
    static const std::string empty = "";
    CCdd *cdd = GetInternalCDDData();
    if (!cdd) return empty;

    // find first 'comment' in Cdd-descr-set, assume this is the "long description"
    if (!cdd->IsSetDescription() || cdd->GetDescription().Get().size() == 0) return empty;
    CCdd_descr_set::Tdata::const_iterator d, de = cdd->GetDescription().Get().end();
    for (d=cdd->GetDescription().Get().begin(); d!=de; d++)
        if ((*d)->IsComment())
            return (*d)->GetComment();

    return empty;
}

bool ASNDataManager::SetCDDDescription(const std::string& descr)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd || descr.size() == 0) return false;

    if (cdd->IsSetDescription() && cdd->GetDescription().Get().size() >= 0) {
        // find first 'comment' in Cdd-descr-set, assume this is the "long description"
        CCdd_descr_set::Tdata::iterator d, de = cdd->SetDescription().Set().end();
        for (d=cdd->SetDescription().Set().begin(); d!=de; d++) {
            if ((*d)->IsComment()) {
                if ((*d)->GetComment() != descr) {
                    (*d)->SetComment(descr);
                    SetDataChanged(eCDDData);
                }
                return true;
            }
        }
    }

    // add new comment if not yet present
    CRef < CCdd_descr > comment(new CCdd_descr);
    comment->SetComment(descr);
    cdd->SetDescription().Set().push_front(comment);
    SetDataChanged(eCDDData);
    return true;
}

bool ASNDataManager::GetCDDNotes(TextLines *lines) const
{
    CCdd *cdd = GetInternalCDDData();
    if (!lines || !cdd) return false;
    lines->clear();

    if (!cdd->IsSetDescription() || cdd->GetDescription().Get().size() == 0)
        return true;

    // find scrapbook item
    CCdd_descr_set::Tdata::const_iterator d, de = cdd->GetDescription().Get().end();
    for (d=cdd->GetDescription().Get().begin(); d!=de; d++) {
        if ((*d)->IsScrapbook()) {

            // fill out lines from scrapbook string list
            lines->resize((*d)->GetScrapbook().size());
            CCdd_descr::TScrapbook::const_iterator l, le = (*d)->GetScrapbook().end();
            int i = 0;
            for (l=(*d)->GetScrapbook().begin(); l!=le; l++)
                (*lines)[i++] = *l;
            return true;
        }
    }
    return true;
}

bool ASNDataManager::SetCDDNotes(const TextLines& lines)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd) return false;

    CCdd_descr::TScrapbook *scrapbook = NULL;

    // find an existing scrapbook item
    if (cdd->IsSetDescription()) {
        CCdd_descr_set::Tdata::iterator d, de = cdd->SetDescription().Set().end();
        for (d=cdd->SetDescription().Set().begin(); d!=de; d++) {
            if ((*d)->IsScrapbook()) {
                if (lines.size() == 0) {
                    cdd->SetDescription().Set().erase(d);   // if empty, remove scrapbook item
                    SetDataChanged(eCDDData);
                    TESTMSG("removed scrapbook");
                } else
                    scrapbook = &((*d)->SetScrapbook());
                break;
            }
        }
    }
    if (lines.size() == 0) return true;

    // create a scrapbook item if doesn't exist already
    if (!scrapbook) {
        CRef < CCdd_descr > descr(new CCdd_descr());
        scrapbook = &(descr->SetScrapbook());
        cdd->SetDescription().Set().push_back(descr);
    }

    // fill out scrapbook lines
    scrapbook->clear();
    for (int i=0; i<lines.size(); i++)
        scrapbook->push_back(lines[i]);
    SetDataChanged(eCDDData);

    return true;
}

ncbi::objects::CCdd_descr_set * ASNDataManager::GetCDDDescrSet(void)
{
    CCdd *cdd = GetInternalCDDData();
    if (cdd) {
        // make a new descr set if not present
        if (!cdd->IsSetDescription()) {
            CRef < CCdd_descr_set > ref(new CCdd_descr_set());
            cdd->SetDescription(ref);
        }
        return &(cdd->SetDescription());
    } else
        return NULL;
}

ncbi::objects::CAlign_annot_set * ASNDataManager::GetCDDAnnotSet(void)
{
    CCdd *cdd = GetInternalCDDData();
    if (cdd) {
        // make a new annot set if not present
        if (!cdd->IsSetAlignannot()) {
			CRef < CAlign_annot_set > ref(new CAlign_annot_set());
            cdd->SetAlignannot(ref);
        }
        return &(cdd->SetAlignannot());
    } else
        return NULL;
}

ncbi::objects::CSeq_id * ASNDataManager::GetCDDMaster3d(void) const
{
    CCdd *cdd = GetInternalCDDData();
    if (cdd && cdd->IsSetMaster3d())
        return cdd->GetMaster3d().front().GetPointer(); // just return the first one...
    else
        return NULL;
}

END_SCOPE(Cn3D)
