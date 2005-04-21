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
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

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
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Global_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include "asn_reader.hpp"
#include "data_manager.hpp"
#include "structure_set.hpp"
#include "sequence_set.hpp"
#include "alignment_set.hpp"
#include "cn3d_tools.hpp"
#include "asn_converter.hpp"

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
    biostrucModelType = eModel_type_ncbi_all_atom;  // default, for CDD's
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

            if (mimeData->GetGeneral().IsSetStructure_type())
                // assume structure-type values match MMDB's Model-type
                biostrucModelType = (EModel_type) mimeData->GetGeneral().GetStructure_type();

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
                    SeqAnnotList::iterator i,
                        ie = mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetImports().end();
                    for (i=mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetImports().begin();
                         i!= ie; ++i) {
                        CRef < CUpdate_align > update(new CUpdate_align());
                        update->SetSeqannot(**i);
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
            ERRORMSG("Unrecognized mime type!");
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
            (mimeData->IsGeneral() &&
                !sequenceAlignments && !structureAlignments &&
                biostrucList && biostrucList->size() == 1)));
    TRACEMSG("is single structure: " << (isSingleStructure ? "yes" : "no"));

    // pre-screen sequence alignments to make sure they're all a type we can deal with
    if (sequenceAlignments) {
        list < CRef < CSeq_align > > validAlignments;
        SeqAnnotList::const_iterator n, ne = sequenceAlignments->end();
        for (n=sequenceAlignments->begin(); n!=ne; ++n) {

            if (!n->GetObject().GetData().IsAlign()) {
                ERRORMSG("Warning - confused by seqannot data format");
                continue;
            }
            if (n != sequenceAlignments->begin()) TRACEMSG("multiple Seq-annots");

            CSeq_annot::C_Data::TAlign::const_iterator
                a, ae = n->GetObject().GetData().GetAlign().end();
            for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; ++a) {

                // verify this is a type of alignment we can deal with
                if (!(a->GetObject().GetType() != CSeq_align::eType_partial ||
                    a->GetObject().GetType() != CSeq_align::eType_diags) ||
                    !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2 ||
                    (!a->GetObject().GetSegs().IsDendiag() && !a->GetObject().GetSegs().IsDenseg())) {
                    ERRORMSG("Warning - confused by alignment type");
                    continue;
                }
                validAlignments.push_back(*a);
            }
        }

        sequenceAlignments->clear();
        if (validAlignments.size() == 0) {
            ERRORMSG("Warning - no valid Seq-aligns present");
            sequenceAlignments = NULL;
        } else {
            sequenceAlignments->push_back(CRef < CSeq_annot > (new CSeq_annot()));
            // copy list of valid alignments only
            sequenceAlignments->front()->SetData().SetAlign() = validAlignments;
        }
    }
}

bool ASNDataManager::ConvertMimeDataToCDD(const std::string& cddName)
{
    // if we have CDD data already, this is really easy
    if (IsCDDInMime()) {
        cddData.Reset(GetInternalCDDData());
    }
    else if (IsCDD()){
        return true;    // nothing to do
    }

    // otherwise, splice together from mime data
    else {
        CRef < CCdd > cdd(new CCdd());
        cdd->SetName(cddName);

        // make up a local id
        static int localID = 1;
        CRef < CCdd_id > id(new CCdd_id());
        cdd->SetId().Set().push_back(id);
        CNcbiOstrstream oss;
        oss << "loc" << localID++ << '\0';
        auto_ptr<char> accession(oss.str());
        id->SetGid().SetAccession(accession.get());

        // fill in data
        if (!seqEntryList) {
            ERRORMSG("can't find sequences to put into new Cdd");
            return false;
        }
        cdd->SetSequences().SetSet().SetSeq_set() = *seqEntryList;
        if (!sequenceAlignments) {
            ERRORMSG("can't find sequence alignments to put into new Cdd");
            return false;
        }
        cdd->SetSeqannot() = *sequenceAlignments;
        if (structureAlignments)
            cdd->SetFeatures(*structureAlignments);
        if (cddUpdates)
            cdd->SetPending() = *cddUpdates;
        if (bundleImportsFaked.size() > 0)
            cdd->SetPending().splice(cdd->SetPending().end(), bundleImportsFaked);

        cddData.Reset(cdd.GetPointer());
    }

    mimeData.Reset();
    Load();
    return true;
}

ASNDataManager::SeqAnnotList * ASNDataManager::GetOrCreateSequenceAlignments(void)
{
    // if necessary, convert mime data into general type that has a place for alignments
    if (!sequenceAlignments && mimeData.NotEmpty() && !mimeData->IsGeneral())
        ConvertMimeToGeneral();

    if (!sequenceAlignments && mimeData.NotEmpty() && mimeData->IsGeneral() &&
            mimeData->GetGeneral().GetSeq_align_data().IsBundle())
        sequenceAlignments =
            &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetSeqaligns());
    return sequenceAlignments;
}

const CCn3d_style_dictionary * ASNDataManager::GetStyleDictionary(void) const
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc() && mimeData->GetAlignstruc().IsSetStyle_dictionary())
            return &(mimeData->GetAlignstruc().GetStyle_dictionary());
        else if (mimeData->IsAlignseq() && mimeData->GetAlignseq().IsSetStyle_dictionary())
            return &(mimeData->GetAlignseq().GetStyle_dictionary());
        else if (mimeData->IsStrucseq() && mimeData->GetStrucseq().IsSetStyle_dictionary())
            return &(mimeData->GetStrucseq().GetStyle_dictionary());
        else if (mimeData->IsStrucseqs() && mimeData->GetStrucseqs().IsSetStyle_dictionary())
            return &(mimeData->GetStrucseqs().GetStyle_dictionary());
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle() &&
                mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetStyle_dictionary())
                return &(mimeData->GetGeneral().GetSeq_align_data().GetBundle().GetStyle_dictionary());
            else if (mimeData->GetGeneral().GetSeq_align_data().IsCdd() &&
                     mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetStyle_dictionary())
                return &(mimeData->GetGeneral().GetSeq_align_data().GetCdd().GetStyle_dictionary());
        }
    } else if (cddData->IsSetStyle_dictionary())
        return &(cddData->GetStyle_dictionary());

    return NULL;
}

void ASNDataManager::SetStyleDictionary(ncbi::objects::CCn3d_style_dictionary& styles)
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

void ASNDataManager::RemoveStyleDictionary(void)
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc()) mimeData->SetAlignstruc().ResetStyle_dictionary();
        else if (mimeData->IsAlignseq()) mimeData->SetAlignseq().ResetStyle_dictionary();
        else if (mimeData->IsStrucseq()) mimeData->SetStrucseq().ResetStyle_dictionary();
        else if (mimeData->IsStrucseqs()) mimeData->SetStrucseqs().ResetStyle_dictionary();
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle())
                mimeData->SetGeneral().SetSeq_align_data().SetBundle().ResetStyle_dictionary();
            else
                mimeData->SetGeneral().SetSeq_align_data().SetCdd().ResetStyle_dictionary();
        }
    } else
        cddData->ResetStyle_dictionary();
}

const CCn3d_user_annotations * ASNDataManager::GetUserAnnotations(void) const
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc() && mimeData->GetAlignstruc().IsSetUser_annotations())
            return &(mimeData->GetAlignstruc().GetUser_annotations());
        else if (mimeData->IsAlignseq() && mimeData->GetAlignseq().IsSetUser_annotations())
            return &(mimeData->GetAlignseq().GetUser_annotations());
        else if (mimeData->IsStrucseq() && mimeData->GetStrucseq().IsSetUser_annotations())
            return &(mimeData->GetStrucseq().GetUser_annotations());
        else if (mimeData->IsStrucseqs() && mimeData->GetStrucseqs().IsSetUser_annotations())
            return &(mimeData->GetStrucseqs().GetUser_annotations());
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle() &&
                mimeData->GetGeneral().GetSeq_align_data().GetBundle().IsSetUser_annotations())
                return &(mimeData->GetGeneral().GetSeq_align_data().GetBundle().GetUser_annotations());
            else if (mimeData->GetGeneral().GetSeq_align_data().IsCdd() &&
                     mimeData->GetGeneral().GetSeq_align_data().GetCdd().IsSetUser_annotations())
                return &(mimeData->GetGeneral().GetSeq_align_data().GetCdd().GetUser_annotations());
        }
    } else if (cddData->IsSetUser_annotations())
        return &(cddData->GetUser_annotations());

    return NULL;
}

void ASNDataManager::SetUserAnnotations(ncbi::objects::CCn3d_user_annotations& annots)
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

void ASNDataManager::RemoveUserAnnotations(void)
{
    if (mimeData.NotEmpty()) {
        if (mimeData->IsAlignstruc()) mimeData->SetAlignstruc().ResetUser_annotations();
        else if (mimeData->IsAlignseq()) mimeData->SetAlignseq().ResetUser_annotations();
        else if (mimeData->IsStrucseq()) mimeData->SetStrucseq().ResetUser_annotations();
        else if (mimeData->IsStrucseqs()) mimeData->SetStrucseqs().ResetUser_annotations();
        else if (mimeData->IsGeneral()) {
            if (mimeData->GetGeneral().GetSeq_align_data().IsBundle())
                mimeData->SetGeneral().SetSeq_align_data().SetBundle().ResetUser_annotations();
            else
                mimeData->SetGeneral().SetSeq_align_data().SetCdd().ResetUser_annotations();
        }
    } else
        cddData->ResetUser_annotations();
}

void ASNDataManager::SetStructureAlignments(ncbi::objects::CBiostruc_annot_set *strucAligns)
{
    if (mimeData.NotEmpty() && mimeData->IsGeneral() && mimeData->GetGeneral().GetSeq_align_data().IsBundle()) {
        if (strucAligns)
            mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetStrucaligns(*strucAligns);
        else
            mimeData->SetGeneral().SetSeq_align_data().SetBundle().ResetStrucaligns();
    } else if (GetInternalCDDData()) {
        if (strucAligns)
            GetInternalCDDData()->SetFeatures(*strucAligns);
        else
            GetInternalCDDData()->ResetFeatures();
    } else
        ERRORMSG("ASNDataManager::SetStructureAlignments() - can't add to this data type");
    structureAlignments = strucAligns;
}

bool ASNDataManager::AddBiostrucToASN(ncbi::objects::CBiostruc *biostruc)
{
    // if can't store add'l Biostrucs currently, try to convert to general mime if possible
    if (!biostrucList && mimeData.NotEmpty() && !mimeData->IsGeneral())
        ConvertMimeToGeneral();

    // add structure to list if there is one
    if (biostrucList) {
        biostrucList->push_back(CRef<CBiostruc>(biostruc));
        return true;
    }

    // if this is a CDD, we never store Biostrucs; but return true to pretend success
    if (IsCDD()) return true;

    return false;
}

bool ASNDataManager::ConvertMimeToGeneral(void)
{
    if (mimeData.Empty() || mimeData->IsGeneral()) {
        ERRORMSG("ASNDataManager::ConvertMimeToGeneral() - can't convert to general mime type");
        return false;
    }

    CNcbi_mime_asn1 *newMime = new CNcbi_mime_asn1();

    // copy structures
    if (biostrucList)
        newMime->SetGeneral().SetStructures() = *biostrucList; // copy list
    if (masterBiostruc) {
        newMime->SetGeneral().SetStructures().push_front(CRef<CBiostruc>(masterBiostruc));
        masterBiostruc = NULL;
    }
    biostrucList = (newMime->GetGeneral().GetStructures().size() > 0) ?
        &(newMime->SetGeneral().SetStructures()) : NULL;

    // copy alignment data into bundle
    CBundle_seqs_aligns *bundle = new CBundle_seqs_aligns();
    newMime->SetGeneral().SetSeq_align_data().SetBundle(*bundle);
    if (seqEntryList) {
        bundle->SetSequences() = *seqEntryList;
        seqEntryList = &(bundle->SetSequences());
    }
    if (structureAlignments)
        bundle->SetStrucaligns(*structureAlignments);
    if (sequenceAlignments) {
        bundle->SetSeqaligns() = *sequenceAlignments;
        sequenceAlignments = &(bundle->SetSeqaligns());
    }

    // install new root mime structure
    mimeData.Reset(newMime);
    SetDataChanged(StructureSet::eOtherData);
    return true;
}

void ASNDataManager::ReplaceUpdates(UpdateAlignList& newUpdates)
{
    // if necessary, convert mime data into general type that has a place for imports
    if (mimeData.NotEmpty() && !mimeData->IsGeneral() && newUpdates.size() > 0)
        ConvertMimeToGeneral();

    // store new updates in asn data
    if (mimeData.NotEmpty() && mimeData->IsGeneral()) {
        if (mimeData->GetGeneral().GetSeq_align_data().IsCdd()) {
            cddUpdates = &(mimeData->SetGeneral().SetSeq_align_data().SetCdd().SetPending());
            *cddUpdates = newUpdates;
        } else {
            bundleImportsFaked = newUpdates;
            // convert to plain imports to store in mime data (some data loss...)
            bundleImports = &(mimeData->SetGeneral().SetSeq_align_data().SetBundle().SetImports());
            bundleImports->clear();
            UpdateAlignList::iterator u, ue = newUpdates.end();
            for (u=newUpdates.begin(); u!=ue; ++u)
                if ((*u)->IsSetSeqannot())
                    bundleImports->push_back(CRef < CSeq_annot > (&((*u)->SetSeqannot())));
        }
    } else if (cddData.NotEmpty()) {
        cddUpdates = &(cddData->SetPending());
        *cddUpdates = newUpdates;
    } else if (newUpdates.size() > 0)
        ERRORMSG("ASNDataManager::ReplaceUpdates() - can't put updates in this data type");

    SetDataChanged(StructureSet::eUpdateData);
}

void ASNDataManager::RemoveUnusedSequences(const AlignmentSet *alignmentSet,
    const SequenceList& updateSequences)
{
    if (!alignmentSet) return; // don't do this for single structures

    if (!seqEntryList) {
        ERRORMSG("ASNDataManager::RemoveUnusedSequences() - can't find sequence list");
        return;
    }

    // update the asn sequences, keeping only those used in the multiple alignment and updates
    seqEntryList->clear();
    map < const MoleculeIdentifier *, bool > usedSeqs;
    int nStructuredSlaves = 0;

// macro to add the sequence to the list if not already present
#define CONDITIONAL_ADD_SEQENTRY(seq) do { \
    if (usedSeqs.find((seq)->identifier) == usedSeqs.end()) { \
        seqEntryList->resize(seqEntryList->size() + 1); \
        seqEntryList->back().Reset(new CSeq_entry); \
        seqEntryList->back().GetObject().SetSeq((const_cast<Sequence*>(seq))->bioseqASN.GetObject()); \
        usedSeqs[(seq)->identifier] = true; \
    } } while (0)

    // always add master first
    CONDITIONAL_ADD_SEQENTRY(alignmentSet->master);
    // add from alignmentSet slaves
    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; ++a) {
        CONDITIONAL_ADD_SEQENTRY((*a)->slave);
        if ((*a)->slave->molecule) ++nStructuredSlaves;
    }

    // add from updates
    SequenceList::const_iterator s, se = updateSequences.end();
    for (s=updateSequences.begin(); s!=se; ++s)
        CONDITIONAL_ADD_SEQENTRY((*s));

    SetDataChanged(StructureSet::eSequenceData);

    // warn user if # structured slaves != # structure alignments
//    if (structureAlignments && nStructuredSlaves !=
//            structureAlignments->GetFeatures().front().GetObject().GetFeatures().size())
//        ERRORMSG("Warning: Structure alignment list does not contain one alignment per "
//            "structured sequence!\nYou should recompute structure alignments before saving "
//            "in order to sync the lists.");
}

bool ASNDataManager::WriteDataToFile(const char *filename, bool isBinary,
    string *err, ncbi::EFixNonPrint fixNonPrint) const
{
    if (mimeData.NotEmpty())
        return WriteASNToFile(filename, mimeData.GetObject(), isBinary, err, fixNonPrint);
    else
        return WriteASNToFile(filename, cddData.GetObject(), isBinary, err, fixNonPrint);
}

ncbi::objects::CCdd * ASNDataManager::GetInternalCDDData(void)
{
    if (cddData.NotEmpty()) return cddData.GetPointer();
    else if (mimeData.NotEmpty() && mimeData->IsGeneral() &&
             mimeData->GetGeneral().GetSeq_align_data().IsCdd())
        return &(mimeData->SetGeneral().SetSeq_align_data().SetCdd());
    else
        return NULL;
}

const ncbi::objects::CCdd * ASNDataManager::GetInternalCDDData(void) const
{
    if (cddData.NotEmpty()) return cddData.GetPointer();
    else if (mimeData.NotEmpty() && mimeData->IsGeneral() &&
             mimeData->GetGeneral().GetSeq_align_data().IsCdd())
        return &(mimeData->GetGeneral().GetSeq_align_data().GetCdd());
    else
        return NULL;
}

bool ASNDataManager::IsCDD(void) const
{
    return (GetInternalCDDData() != NULL);
}

bool ASNDataManager::IsCDDInMime(void) const
{
    return (GetInternalCDDData() != NULL && mimeData.NotEmpty());
}

const string& ASNDataManager::GetCDDName(void) const
{
    static const string empty = "";
    const CCdd *cdd = GetInternalCDDData();
    if (cdd)
        return cdd->GetName();
    else
        return empty;
}

bool ASNDataManager::SetCDDName(const string& name)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd || name.size() == 0) return false;
    cdd->SetName(name);
    SetDataChanged(StructureSet::eCDDData);
    return true;
}

const string& ASNDataManager::GetCDDDescription(void) const
{
    static const string empty = "";
    const CCdd *cdd = GetInternalCDDData();
    if (!cdd) return empty;

    // find first 'comment' in Cdd-descr-set, assume this is the "long description"
    if (!cdd->IsSetDescription() || cdd->GetDescription().Get().size() == 0) return empty;
    CCdd_descr_set::Tdata::const_iterator d, de = cdd->GetDescription().Get().end();
    for (d=cdd->GetDescription().Get().begin(); d!=de; ++d)
        if ((*d)->IsComment())
            return (*d)->GetComment();

    return empty;
}

bool ASNDataManager::SetCDDDescription(const string& descr)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd || descr.size() == 0) return false;

    if (cdd->IsSetDescription() && cdd->GetDescription().Get().size() >= 0) {
        // find first 'comment' in Cdd-descr-set, assume this is the "long description"
        CCdd_descr_set::Tdata::iterator d, de = cdd->SetDescription().Set().end();
        for (d=cdd->SetDescription().Set().begin(); d!=de; ++d) {
            if ((*d)->IsComment()) {
                if ((*d)->GetComment() != descr) {
                    (*d)->SetComment(descr);
                    SetDataChanged(StructureSet::eCDDData);
                }
                return true;
            }
        }
    }

    // add new comment if not yet present
    CRef < CCdd_descr > comment(new CCdd_descr);
    comment->SetComment(descr);
    cdd->SetDescription().Set().push_front(comment);
    SetDataChanged(StructureSet::eCDDData);
    return true;
}

bool ASNDataManager::GetCDDNotes(TextLines *lines) const
{
    const CCdd *cdd = GetInternalCDDData();
    if (!lines || !cdd) return false;
    lines->clear();

    if (!cdd->IsSetDescription() || cdd->GetDescription().Get().size() == 0)
        return true;

    // find scrapbook item
    CCdd_descr_set::Tdata::const_iterator d, de = cdd->GetDescription().Get().end();
    for (d=cdd->GetDescription().Get().begin(); d!=de; ++d) {
        if ((*d)->IsScrapbook()) {

            // fill out lines from scrapbook string list
            lines->resize((*d)->GetScrapbook().size());
            CCdd_descr::TScrapbook::const_iterator l, le = (*d)->GetScrapbook().end();
            int i = 0;
            for (l=(*d)->GetScrapbook().begin(); l!=le; ++l)
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
        for (d=cdd->SetDescription().Set().begin(); d!=de; ++d) {
            if ((*d)->IsScrapbook()) {
                if (lines.size() == 0) {
                    cdd->SetDescription().Set().erase(d);   // if empty, remove scrapbook item
                    SetDataChanged(StructureSet::eCDDData);
                    TRACEMSG("removed scrapbook");
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
    for (int i=0; i<lines.size(); ++i)
        scrapbook->push_back(lines[i]);
    SetDataChanged(StructureSet::eCDDData);

    return true;
}

ncbi::objects::CCdd_descr_set * ASNDataManager::GetCDDDescrSet(void)
{
    CCdd *cdd = GetInternalCDDData();
    if (cdd) {
        // make a new descr set if not present
        if (!cdd->IsSetDescription()) {
            CRef < CCdd_descr_set > ref(new CCdd_descr_set());
            cdd->SetDescription(*ref);
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
            cdd->SetAlignannot(*ref);
        }
        return &(cdd->SetAlignannot());
    } else
        return NULL;
}

const ncbi::objects::CSeq_id * ASNDataManager::GetCDDMaster3d(void) const
{
    const CCdd *cdd = GetInternalCDDData();
    if (cdd && cdd->IsSetMaster3d())
        return cdd->GetMaster3d().front().GetPointer(); // just return the first one...
    else
        return NULL;
}

void ASNDataManager::AddReject(ncbi::objects::CReject_id *reject)
{
    CCdd *cdd = GetInternalCDDData();
    if (!cdd) return;

    cdd->SetRejects().push_back(CRef < CReject_id > (reject));
    dataChanged |= StructureSet::eCDDData;
}

const StructureSet::RejectList * ASNDataManager::GetRejects(void) const
{
    const CCdd *cdd = GetInternalCDDData();
    if (!cdd || !cdd->IsSetRejects())
        return NULL;
    return &(cdd->GetRejects());
}

// the following three Check... functions are taken from CDTree2

bool CheckOneDenDiag(CRef<CDense_diag>& DenDiag, string& Str) {
//---------------------------------------------------------------------------
// check integrity of DenDiag.
//---------------------------------------------------------------------------
    int  Start1, Start2, Len, Dim;
    char buf[1024];
    bool RetVal = true;

    // check that Dim of this dense-diag is exactly 2
    Dim = DenDiag->GetDim();
    if (Dim != 2) {
        sprintf(buf, "Dense-diag Dim = %d. It should always be exactly 2.\n", Dim);
        RetVal = false;
    }

    // check that Len of this dense-diag is at least 1
    Len = DenDiag->GetLen();
    if (Len < 0) {
        sprintf(buf, "Dense-diag Len = %d. It should always be non-negative.\n", Len);
        RetVal = false;
    }

    // check that Start of first row is sensible.
    Start1 = DenDiag->GetStarts().front();
    if (Start1 < 0) {
        sprintf(buf, "Dense-diag has 1st row Start %d. It should always be non-negative.\n", Start1);
        RetVal = false;
    }

    // check that Start of second row is sensible.
    Start2 = DenDiag->GetStarts().back();
    if (Start2 < 0) {
        sprintf(buf, "Dense-diag has 2nd row start %d. It should always be non-negative.\n", Start2);
        RetVal = false;
    }

    if (!RetVal) {
        Str += buf;
    }
    return(RetVal);
}

bool CheckTwoDenDiags(CRef<CDense_diag>& DenDiag1, CRef<CDense_diag>& DenDiag2, string& Str) {
//---------------------------------------------------------------------------
// check that DenDiag2 follows DenDiag1.
//---------------------------------------------------------------------------
    int  Start11, Start12, Start21, Start22, Len;
    char buf[1024];
    bool RetVal = true;

    // get starts for first den-diag, and get its len
    Start11 = DenDiag1->GetStarts().front();
    Start12 = DenDiag1->GetStarts().back();
    Len = DenDiag1->GetLen();

    // get starts for second den-diag
    Start21 = DenDiag2->GetStarts().front();
    Start22 = DenDiag2->GetStarts().back();

    // make sure starts for second den-diag follow starts for first
    if (Start21 < (Start11 + Len)) {
        sprintf(buf, "Dense-diag has 1st row Start %d, Len %d. Next Dense-diag has Start %d.\n", Start11, Len, Start21);
        RetVal = false;
    }

    if (Start22 < (Start12 + Len)) {
        sprintf(buf, "Dense-diag has 2nd row Start %d, Len %d. Next Dense-diag has Start %d.\n", Start12, Len, Start22);
        RetVal = false;
    }

    if (!RetVal) {
        Str += buf;
    }
    return(RetVal);
}

bool CheckOneAlignment(const CRef<CSeq_align>& SeqAlign, string& Str) {
//---------------------------------------------------------------------------
// check integrity of one aligment
//---------------------------------------------------------------------------
    list< CRef< CDense_diag > >            DenDiags;
    list< CRef< CDense_diag > >::iterator  dd_iter;
          CRef< CDense_diag >              DenDiag1, DenDiag2;
    bool  RetVal = true;

    // if alignment has no dense-diags go to next alignment
    if (!SeqAlign->GetSegs().IsDendiag()) return(RetVal);

    // get den-diags of alignment
    DenDiags = SeqAlign->GetSegs().GetDendiag();

    // get first den-diag
    dd_iter = DenDiags.begin();
    DenDiag1 = *dd_iter;

    // check integrity of first den-diag
    if (!CheckOneDenDiag(DenDiag1, Str)) RetVal = false;

    // handle case when there's just one den-diag in the alignment
    if (dd_iter == DenDiags.end()) return(RetVal);
    dd_iter++;

    // loop through the den-diags.  make sure we're ascending.
    for (; dd_iter != DenDiags.end(); dd_iter++) {
        // get next den-diag
        DenDiag2 = *dd_iter;
        // check integrity of this den-diag
        if (!CheckOneDenDiag(DenDiag2, Str)) RetVal = false;
        // check that this den-diag is later in the sequence than preceeding den-diag
        if (!CheckTwoDenDiags(DenDiag1, DenDiag2, Str)) RetVal = false;
        DenDiag1 = DenDiag2;
    }
    return(RetVal);
}

bool ASNDataManager::MonitorAlignments(void) const
{
    bool okay = true;
    string messages;
    char buf[1024];

    // check regular alignments
    const SeqAnnotList *seqAnnots = GetSequenceAlignments();
    if (seqAnnots) {
        SeqAnnotList::const_iterator a, ae = seqAnnots->end();
        for (a=seqAnnots->begin(); a!=ae; ++a) {
            if ((*a)->GetData().IsAlign()) {
                CSeq_annot::C_Data::TAlign::const_iterator s, se = (*a)->GetData().GetAlign().end();
                int n = 1;
                for (s=(*a)->GetData().GetAlign().begin(); s!=se; ++s, ++n) {
                    if (!CheckOneAlignment(*s, messages)) {
                        okay = false;
                        sprintf(buf, "Normal Alignment #%i\n\n", n);
                        messages += buf;
                    }
                }
            }
        }
    }

    // check update alignments
    const UpdateAlignList *updates = GetUpdates();
    if (updates) {
        UpdateAlignList::const_iterator u, ue = updates->end();
        for (u=updates->begin(); u!=ue; ++u) {
            if ((*u)->IsSetSeqannot() && (*u)->GetSeqannot().GetData().IsAlign()) {
                CSeq_annot::C_Data::TAlign::const_iterator s, se = (*u)->GetSeqannot().GetData().GetAlign().end();
                int n = 1;
                for (s=(*u)->GetSeqannot().GetData().GetAlign().begin(); s!=se; ++s, ++n) {
                    if (!CheckOneAlignment(*s, messages)) {
                        okay = false;
                        sprintf(buf, "Pending Alignment #%i\n\n", n);
                        messages += buf;
                    }
                }
            }
        }
    }

    if (okay)
        TRACEMSG("MonitorAlignments() succeeded");
    else {
        ERRORMSG("MonitorAlignments() found errors, see log for details; please copy&paste and send a complete report to cdt-swg");
        WARNINGMSG("MonitorAlignments() found errors:\n\n" << messages);
    }
    return okay;
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2005/04/21 14:31:19  thiessen
* add MonitorAlignments()
*
* Revision 1.23  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.22  2004/05/21 17:29:51  thiessen
* allow conversion of mime to cdd data
*
* Revision 1.21  2004/03/15 18:25:36  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.20  2004/02/19 17:04:55  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.19  2003/08/21 18:02:01  thiessen
* change header order for Mac compilation
*
* Revision 1.18  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.17  2003/01/27 15:52:22  thiessen
* merge after highlighted row; show rejects; trim rejects from new reject list
*
* Revision 1.16  2002/11/22 19:48:14  thiessen
* one more fix for const changes in objects API
*
* Revision 1.15  2002/11/19 21:19:44  thiessen
* more const changes for objects; fix user vs default style bug
*
* Revision 1.14  2002/11/18 19:48:40  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.13  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.12  2002/10/27 22:23:51  thiessen
* save structure alignments from vastalign.cgi imports
*
* Revision 1.11  2002/07/03 13:39:39  thiessen
* fix for redundant sequence removal
*
* Revision 1.10  2002/06/05 17:50:08  thiessen
* title tweaks
*
* Revision 1.9  2002/03/19 18:47:58  thiessen
* small bug fixes; remember PSSM weight
*
* Revision 1.8  2002/02/27 16:29:41  thiessen
* add model type flag to general mime type
*
* Revision 1.7  2002/02/19 14:59:39  thiessen
* add CDD reject and purge sequence
*
* Revision 1.6  2002/02/12 17:19:21  thiessen
* first working structure import
*
* Revision 1.5  2002/01/19 02:34:40  thiessen
* fixes for changes in asn serialization API
*
* Revision 1.4  2001/12/15 03:15:59  thiessen
* adjustments for slightly changed object loader Set...() API
*
* Revision 1.3  2001/12/06 23:13:45  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.2  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.1  2001/11/27 16:26:08  thiessen
* major update to data management system
*
*/
