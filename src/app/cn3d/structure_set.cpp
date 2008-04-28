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
*      Classes to hold sets of structure data
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbistl.hpp>

#include <deque>

#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/ncbimime/Biostruc_seqs.hpp>
#include <objects/ncbimime/Biostruc_align.hpp>
#include <objects/ncbimime/Entrez_general.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Biostruc_feature_id.hpp>
#include <objects/mmdb3/Chem_graph_alignment.hpp>
#include <objects/mmdb3/Transform.hpp>
#include <objects/mmdb3/Move.hpp>
#include <objects/mmdb3/Trans_matrix.hpp>
#include <objects/mmdb3/Rot_matrix.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/cn3d/Cn3d_style_dictionary.hpp>
#include <objects/cn3d/Cn3d_user_annotations.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/mmdb3/Biostruc_feature_set_id.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/cdd/Reject_id.hpp>
#include <objects/cdd/Update_comment.hpp>

#include "remove_header_conflicts.hpp"

#include "structure_set.hpp"
#include "data_manager.hpp"
#include "coord_set.hpp"
#include "chemical_graph.hpp"
#include "atom_set.hpp"
#include "opengl_renderer.hpp"
#include "show_hide_manager.hpp"
#include "style_manager.hpp"
#include "sequence_set.hpp"
#include "alignment_set.hpp"
#include "alignment_manager.hpp"
#include "messenger.hpp"
#include "block_multiple_alignment.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_cache.hpp"
#include "molecule.hpp"
#include "residue.hpp"
#include "show_hide_dialog.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

const unsigned int
    StructureSet::ePSSMData                   = 0x01,
    StructureSet::eRowOrderData               = 0x02,
    StructureSet::eAnyAlignmentData           = 0x04,
    StructureSet::eStructureAlignmentData     = 0x08,
    StructureSet::eSequenceData               = 0x10,
    StructureSet::eUpdateData                 = 0x20,
    StructureSet::eStyleData                  = 0x40,
    StructureSet::eUserAnnotationData         = 0x80,
    StructureSet::eCDDData                    = 0x100,
    StructureSet::eOtherData                  = 0x200;

const unsigned int
    StructureSet::eSelectProtein              = 0x01,
    StructureSet::eSelectNucleotide           = 0x02,
    StructureSet::eSelectHeterogen            = 0x04,
    StructureSet::eSelectSolvent              = 0x08,
    StructureSet::eSelectOtherMoleculesOnly   = 0x10;

StructureSet::StructureSet(CNcbi_mime_asn1 *mime, unsigned int structureLimit, OpenGLRenderer *r) :
    StructureBase(NULL), renderer(r)
{
    dataManager = new ASNDataManager(mime);
    Load(structureLimit);
}

StructureSet::StructureSet(CCdd *cdd, unsigned int structureLimit, OpenGLRenderer *r) :
    StructureBase(NULL), renderer(r)
{
    dataManager = new ASNDataManager(cdd);
    Load(structureLimit);
}

void StructureSet::LoadSequencesForSingleStructure(void)
{
    sequenceSet = new SequenceSet(this, *(dataManager->GetSequences()));

    if (objects.size() > 1)
        ERRORMSG("LoadSequencesForSingleStructure() called, but there is > 1 structure");
    if (objects.size() != 1) return;

    // look for biopolymer molecules
    ChemicalGraph::MoleculeMap::const_iterator m, me = objects.front()->graph->molecules.end();
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet->sequences.end();
    for (m=objects.front()->graph->molecules.begin(); m!=me; ++m) {
        if (!m->second->IsProtein() && !m->second->IsNucleotide()) continue;

        // find matching sequence for each biopolymer
        for (s=sequenceSet->sequences.begin(); s!=se; ++s) {
            if ((*s)->molecule != NULL) continue; // skip already-matched sequences

            if (m->second->identifier == (*s)->identifier) {

                // verify length
                if (m->second->residues.size() != (*s)->Length()) {
                    ERRORMSG(
                        "LoadSequencesForSingleStructure() - length mismatch between sequence gi "
                        << "and matching molecule " << m->second->identifier->ToString());
                    continue;
                }
                TRACEMSG("matched sequence " << " gi " << (*s)->identifier->gi << " with object "
                    << objects.front()->pdbID << " moleculeID " << m->second->id);

                (const_cast<Molecule*>(m->second))->sequence = *s;
                (const_cast<Sequence*>(*s))->molecule = m->second;
                break;
            }
        }
        if (s == se)
            ERRORMSG("LoadSequencesForSingleStructure() - can't find sequence for molecule "
                << m->second->identifier->ToString());
    }
}

bool StructureSet::LoadMaster(int masterMMDBID)
{
    if (objects.size() > 0) return false;
    TRACEMSG("loading master " << masterMMDBID);

    if (dataManager->GetMasterStructure()) {
        objects.push_back(new StructureObject(this, *(dataManager->GetMasterStructure()), true));
        if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && objects.front()->mmdbID != masterMMDBID)
            ERRORMSG("StructureSet::LoadMaster() - mismatched master MMDB ID");
    } else if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && dataManager->GetStructureList()) {
        ASNDataManager::BiostrucList::const_iterator b, be = dataManager->GetStructureList()->end();
        for (b=dataManager->GetStructureList()->begin(); b!=be; ++b) {
            if ((*b)->GetId().front()->IsMmdb_id() &&
                (*b)->GetId().front()->GetMmdb_id().Get() == masterMMDBID) {
                objects.push_back(new StructureObject(this, **b, true));
                usedStructures[b->GetPointer()] = true;
                break;
            }
        }
    }
    if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && objects.size() == 0) {
        CRef < CBiostruc > biostruc;
        wxString id;
        id.Printf("%i", masterMMDBID);
        if (LoadStructureViaCache(id.c_str(), dataManager->GetBiostrucModelType(), biostruc, NULL))
            objects.push_back(new StructureObject(this, *biostruc, true));
    }
    return (objects.size() > 0);
}

bool StructureSet::MatchSequenceToMoleculeInObject(const Sequence *seq,
    const StructureObject *obj, const Sequence **seqHandle)
{
    ChemicalGraph::MoleculeMap::const_iterator m, me = obj->graph->molecules.end();
    for (m=obj->graph->molecules.begin(); m!=me; ++m) {
        if (!(m->second->IsProtein() || m->second->IsNucleotide())) continue;

        if (m->second->identifier == seq->identifier) {

            // verify length
            if (m->second->residues.size() != seq->Length()) {
                ERRORMSG(
                    "MatchSequenceToMoleculeInObject() - length mismatch between sequence gi "
                    << "and matching molecule " << m->second->identifier->ToString());
                continue;
            }
            TRACEMSG("matched sequence " << " gi " << seq->identifier->gi << " with object "
                << obj->pdbID << " moleculeID " << m->second->id);

            // sanity check
            if (m->second->sequence) {
                ERRORMSG("Molecule " << m->second->identifier->ToString()
                    << " already has an associated sequence");
                continue;
            }

            // automatically duplicate Sequence if it's already associated with a molecule
            if (seq->molecule) {
                TRACEMSG("duplicating sequence " << seq->identifier->ToString());
				SequenceSet *seqSetMod = const_cast<SequenceSet*>(sequenceSet);
                CBioseq& bioseqMod = const_cast<CBioseq&>(seq->bioseqASN.GetObject());
                seq = new Sequence(seqSetMod, bioseqMod);
                seqSetMod->sequences.push_back(seq);
                // update Sequence handle, which should be a handle to a MasterDependentAlignment dependent,
                // so that this new Sequence* is correctly loaded into the BlockMultipleAlignment
                if (seqHandle) *seqHandle = seq;
            }

            // do the cross-match
            (const_cast<Molecule*>(m->second))->sequence = seq;
            (const_cast<Sequence*>(seq))->molecule = m->second;
            break;
        }
    }
    return (m != me);
}

static void SetStructureRowFlags(const AlignmentSet *alignmentSet, unsigned int *structureLimit,
    vector < bool > *dontLoadRowStructure)
{
    vector < string > titles;
    vector < unsigned int > rows;

    // find dependent rows with associated structure
    AlignmentSet::AlignmentList::const_iterator l, le = alignmentSet->alignments.end();
    unsigned int row;
    for (l=alignmentSet->alignments.begin(), row=0; l!=le; ++l, ++row) {
        if ((*l)->dependent->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET) {
            titles.push_back((*l)->dependent->identifier->ToString());
            rows.push_back(row);
        }
    }

    if (*structureLimit - 1 >= titles.size()) return;

    // let user select which dependents to load
    wxString *items = new wxString[titles.size()];
    vector < bool > itemsOn(titles.size(), false);
    for (row=0; row<titles.size(); ++row) {
        items[row] = titles[row].c_str();
        if (row < *structureLimit - 1)      // by default, first N-1 are selected
            itemsOn[row] = true;
    }
    ShowHideDialog dialog(items, &itemsOn, NULL, true, NULL, -1, "Choose structures:");

    if (dialog.ShowModal() == wxOK) {
        // figure out which rows the user selected, and adjust structureLimit accordingly
        *structureLimit = 1;     // master always visible
        for (row=0; row<itemsOn.size(); ++row) {
            if (itemsOn[row])
                (*structureLimit)++;                        // structure should be loaded
            else
                (*dontLoadRowStructure)[rows[row]] = true;  // structure should not be loaded
        }
    }

    delete[] items;
}

void StructureSet::LoadAlignmentsAndStructures(unsigned int structureLimit)
{
    // try to determine the master structure
    int masterMMDBID = MoleculeIdentifier::VALUE_NOT_SET;
    // explicit master structure
    if (dataManager->GetMasterStructure() &&
        dataManager->GetMasterStructure()->GetId().front()->IsMmdb_id())
        masterMMDBID = dataManager->GetMasterStructure()->GetId().front()->GetMmdb_id().Get();
    // master of structure alignments
    else if (dataManager->GetStructureAlignments() &&
                dataManager->GetStructureAlignments()->IsSetId() &&
                dataManager->GetStructureAlignments()->GetId().front()->IsMmdb_id())
        masterMMDBID = dataManager->GetStructureAlignments()->GetId().front()->GetMmdb_id().Get();
    // SPECIAL CASE: if there's no explicit master structure, but there is a structure
    // list that contains a single structure, then assume that it is the master structure
    else if (dataManager->GetStructureList() &&
                dataManager->GetStructureList()->size() == 1 &&
                dataManager->GetStructureList()->front()->GetId().front()->IsMmdb_id())
        masterMMDBID = dataManager->GetStructureList()->front()->GetId().front()->GetMmdb_id().Get();

    // assume data manager has already screened the alignment list
    ASNDataManager::SeqAnnotList *alignments = dataManager->GetSequenceAlignments();
    typedef list < CRef < CSeq_align > > SeqAlignList;
    const SeqAlignList& seqAligns = alignments->front()->GetData().GetAlign();

    // we need to determine the identity of the master sequence; most rigorous way is to look
    // for a Seq-id that is present in all pairwise alignments
    const Sequence *seq1 = NULL, *seq2 = NULL, *master = NULL;
    bool seq1PresentInAll = true, seq2PresentInAll = true;

    // first, find sequences for first pairwise alignment
    const CSeq_id& frontSid = seqAligns.front()->GetSegs().IsDendiag() ?
        seqAligns.front()->GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
        seqAligns.front()->GetSegs().GetDenseg().GetIds().front().GetObject();
    const CSeq_id& backSid = seqAligns.front()->GetSegs().IsDendiag() ?
        seqAligns.front()->GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
        seqAligns.front()->GetSegs().GetDenseg().GetIds().back().GetObject();
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet->sequences.end();
    for (s=sequenceSet->sequences.begin(); s!=se; ++s) {
        if ((*s)->identifier->MatchesSeqId(frontSid)) seq1 = *s;
        if ((*s)->identifier->MatchesSeqId(backSid)) seq2 = *s;
        if (seq1 && seq2) break;
    }
    if (!(seq1 && seq2)) {
        ERRORMSG("Can't match first pair of Seq-ids to Sequences");
        return;
    }

    // now, make sure one of these sequences is present in all the other pairwise alignments
    SeqAlignList::const_iterator a = seqAligns.begin(), ae = seqAligns.end();
    for (++a; a!=ae; ++a) {
        const CSeq_id& frontSid2 = (*a)->GetSegs().IsDendiag() ?
            (*a)->GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
            (*a)->GetSegs().GetDenseg().GetIds().front().GetObject();
        const CSeq_id& backSid2 = (*a)->GetSegs().IsDendiag() ?
            (*a)->GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
            (*a)->GetSegs().GetDenseg().GetIds().back().GetObject();
        if (!seq1->identifier->MatchesSeqId(frontSid2) && !seq1->identifier->MatchesSeqId(backSid2))
            seq1PresentInAll = false;
        if (!seq2->identifier->MatchesSeqId(frontSid2) && !seq2->identifier->MatchesSeqId(backSid2))
            seq2PresentInAll = false;
    }
    if (!seq1PresentInAll && !seq2PresentInAll) {
        ERRORMSG("All pairwise sequence alignments must have a common master sequence");
        return;
    } else if (seq1PresentInAll && !seq2PresentInAll)
        master = seq1;
    else if (seq2PresentInAll && !seq1PresentInAll)
        master = seq2;
    else if (seq1PresentInAll && seq2PresentInAll && seq1 == seq2)
        master = seq1;

    // if still ambiguous, see if master3d is set in CDD data
    if (!master && dataManager->GetCDDMaster3d()) {
        if (seq1->identifier->MatchesSeqId(*(dataManager->GetCDDMaster3d())))
            master = seq1;
        else if (seq2->identifier->MatchesSeqId(*(dataManager->GetCDDMaster3d())))
            master = seq2;
        else
            ERRORMSG("Unable to match CDD's master3d with either sequence in first pairwise alignment");
    }

    // if still ambiguous, try to use master structure info to find master sequence
    if (!master && masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET) {
        // load master - has side affect of matching gi's with PDB/molecule ID during graph evaluation
        if (structureLimit > 0)
            LoadMaster(masterMMDBID);

        // see if there's a sequence in the master structure that matches
        if (seq1->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
            seq1->identifier->mmdbID != seq2->identifier->mmdbID) {
            if (masterMMDBID == seq1->identifier->mmdbID)
                master = seq1;
            else if (masterMMDBID == seq2->identifier->mmdbID)
                master = seq2;
            else {
                ERRORMSG("Structure master does not contain either sequence in first pairwise alignment");
                return;
            }
        }
    }

    // if still ambiguous, just use the first one
    if (!master) {
        WARNINGMSG("Ambiguous master; using " << seq1->identifier->ToString());
        master = seq1;
    }

    TRACEMSG("determined that master sequence is " << master->identifier->ToString());

    // load alignments now that we know the identity of the master
    alignmentSet = new AlignmentSet(this, master, *(dataManager->GetSequenceAlignments()));

    // check mmdb id's and load master if not already present (and if master has structure)
    if (masterMMDBID == MoleculeIdentifier::VALUE_NOT_SET) {
        masterMMDBID = master->identifier->mmdbID;
    } else if (master->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
               master->identifier->mmdbID != masterMMDBID) {
        ERRORMSG("master structure (" << masterMMDBID <<
            ") disagrees with master sequence (apparently from " << master->identifier->mmdbID << ')');
        return;
    }
    if (objects.size() == 0 && structureLimit > 0 && masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET &&
            master->identifier->GetLabel() != "consensus")   // special case for looking at "raw" CD's
        LoadMaster(masterMMDBID);

    // cross-match master sequence and structure
    if (objects.size() == 1 && !MatchSequenceToMoleculeInObject(master, objects.front())) {
        ERRORMSG("MatchSequenceToMoleculeInObject() - can't find molecule in object "
            << objects.front()->pdbID << " to match master sequence "
            << master->identifier->ToString());
        return;
    }

    // IFF there's a master structure, then also load dependent structures and cross-match sequences
    if (objects.size() == 1 && structureLimit > 1) {
        ASNDataManager::BiostrucList::const_iterator b, be;
        if (dataManager->GetStructureList()) be = dataManager->GetStructureList()->end();
        int row;
        vector < bool > loadedStructureForDependentRow(alignmentSet->alignments.size(), false);

        // first, load each remaining dependent structure, and for each one, find the first dependent
        // sequence that matches it (and that doesn't already have structure)
        AlignmentSet::AlignmentList::const_iterator l, le = alignmentSet->alignments.end();
        if (dataManager->GetStructureList()) {
            for (b=dataManager->GetStructureList()->begin(); b!=be && objects.size()<structureLimit; ++b) {

                // load structure
                if (usedStructures.find(b->GetPointer()) != usedStructures.end()) continue;
                StructureObject *object = new StructureObject(this, **b, false);
                objects.push_back(object);
                if (dataManager->GetStructureAlignments())
                    object->SetTransformToMaster(
                        *(dataManager->GetStructureAlignments()), master->identifier->mmdbID);
                usedStructures[b->GetPointer()] = true;

                // find matching unstructured dependent sequence
                for (l=alignmentSet->alignments.begin(), row=0; l!=le; ++l, ++row) {
                    if (loadedStructureForDependentRow[row]) continue;
                    if (MatchSequenceToMoleculeInObject((*l)->dependent, object,
                            &((const_cast<MasterDependentAlignment*>(*l))->dependent))) {
                        loadedStructureForDependentRow[row] = true;
                        break;
                    }
                }
                if (l == le)
                    ERRORMSG("Warning: Structure " << object->pdbID
                        << " doesn't have a matching dependent sequence in the multiple alignment");
            }
        }

        // now loop through dependent rows of the alignment; if the dependent
        // sequence has an MMDB ID but no structure yet, then load it.
        if (objects.size() < structureLimit && (dataManager->IsCDD() || dataManager->IsGeneralMime())) {

            // for CDD's, ask user which structures to load if structureLimit is low
            if (dataManager->IsCDD())
                SetStructureRowFlags(alignmentSet, &structureLimit, &loadedStructureForDependentRow);

            for (l=alignmentSet->alignments.begin(), row=0; l!=le && objects.size()<structureLimit; ++l, ++row) {

                if ((*l)->dependent->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
                    !loadedStructureForDependentRow[row]) {

                    // first check the biostruc list to see if this structure is present already
                    CRef < CBiostruc > biostruc;
                    if (dataManager->GetStructureList()) {
                        for (b=dataManager->GetStructureList()->begin(); b!=be ; ++b) {
                            if ((*b)->GetId().front()->IsMmdb_id() &&
                                (*b)->GetId().front()->GetMmdb_id().Get() == (*l)->dependent->identifier->mmdbID) {
                                biostruc = *b;
                                break;
                            }
                        }
                    }

                    // if not in list, load Biostruc via HTTP/cache
                    if (biostruc.Empty()) {
                        wxString id;
                        id.Printf("%i", (*l)->dependent->identifier->mmdbID);
                        if (!LoadStructureViaCache(id.c_str(),
                                dataManager->GetBiostrucModelType(), biostruc, NULL)) {
                            ERRORMSG("Failed to load MMDB #" << (*l)->dependent->identifier->mmdbID);
                            continue;
                        }
                    }

                    // create StructureObject and cross-match
                    StructureObject *object = new StructureObject(this, *biostruc, false);
                    objects.push_back(object);
                    if (dataManager->GetStructureAlignments())
                        object->SetTransformToMaster(
                            *(dataManager->GetStructureAlignments()), master->identifier->mmdbID);
                    if (!MatchSequenceToMoleculeInObject((*l)->dependent, object,
                            &((const_cast<MasterDependentAlignment*>(*l))->dependent)))
                        ERRORMSG("Failed to match any molecule in structure " << object->pdbID
                            << " with sequence " << (*l)->dependent->identifier->ToString());
                    loadedStructureForDependentRow[row] = true;
                }
            }
        }
    }
}

void StructureSet::Load(unsigned int structureLimit)
{
    // member data initialization
    lastAtomName = OpenGLRenderer::NO_NAME;
    lastDisplayList = OpenGLRenderer::NO_LIST;
    sequenceSet = NULL;
    alignmentSet = NULL;
    alignmentManager = NULL;
    nDomains = 0;
    isAlphaOnly = false;
    parentSet = this;
    showHideManager = new ShowHideManager();
    styleManager = new StyleManager(this);
    havePrevPickedAtomCoord = false;
    hasUserStyle = false;

    // if this is a single structure, then there should be one sequence per biopolymer
    if (dataManager->IsSingleStructure()) {
        const CBiostruc *masterBiostruc = dataManager->GetMasterStructure();
        if (!masterBiostruc && dataManager->GetStructureList() && dataManager->GetStructureList()->size() == 1)
            masterBiostruc = dataManager->GetStructureList()->front().GetPointer();
        if (masterBiostruc)
            objects.push_back(new StructureObject(this, *masterBiostruc, true));
        if (dataManager->GetSequences())
            LoadSequencesForSingleStructure();
    }

    // multiple structure: should have exactly one sequence per structure (plus unstructured sequences)
    else {
        if (!dataManager->GetSequences() || !dataManager->GetSequenceAlignments()) {
            ERRORMSG("Data interpreted as multiple alignment, "
                "but missing sequences and/or sequence alignments");
            return;
        }
        sequenceSet = new SequenceSet(this, *(dataManager->GetSequences()));
        LoadAlignmentsAndStructures(structureLimit);
    }

    // find center of coordinates
    SetCenter();

    // create alignment manager
    if (sequenceSet) {
        if (dataManager->GetUpdates())
            // if updates present, alignment manager will load those into update viewer
            alignmentManager = new AlignmentManager(sequenceSet, alignmentSet, *(dataManager->GetUpdates()));
        else
            alignmentManager = new AlignmentManager(sequenceSet, alignmentSet);
    }

    VerifyFrameMap();

    // setup show/hide items
    showHideManager->ConstructShowHideArray(this);
    // alignments always start with aligned domains only
    if (alignmentSet)
        showHideManager->ShowAlignedDomains(this);

    // load style dictionary and user annotations
    const CCn3d_style_dictionary *styles = dataManager->GetStyleDictionary();
    if (styles) {
        if (!styleManager->LoadFromASNStyleDictionary(*styles) ||
            !styleManager->CheckGlobalStyleSettings())
            ERRORMSG("Error loading style dictionary");
        dataManager->RemoveStyleDictionary();   // remove now; recreated with current settings upon save
        hasUserStyle = true;
    }

    const CCn3d_user_annotations *annots = dataManager->GetUserAnnotations();
    if (annots) {
        if (!styleManager->LoadFromASNUserAnnotations(*annots) ||
            !renderer->LoadFromASNViewSettings(*annots))
            ERRORMSG("Error loading user annotations or camera settings");
        dataManager->RemoveUserAnnotations();   // remove now; recreated with current settings upon save
    }

    dataManager->SetDataUnchanged();
}

StructureSet::~StructureSet(void)
{
    delete dataManager;
    delete showHideManager;
    delete styleManager;
    if (alignmentManager) delete alignmentManager;

    GlobalMessenger()->RemoveAllHighlights(false);
    MoleculeIdentifier::ClearIdentifiers();
}

bool StructureSet::AddBiostrucToASN(ncbi::objects::CBiostruc *biostruc)
{
    bool added = dataManager->AddBiostrucToASN(biostruc);
    if (added && objects.size() == 1)
        InitStructureAlignments(objects.front()->mmdbID);
    return added;
}

static const int NO_DOMAIN = -1, MULTI_DOMAIN = 0;

void StructureSet::InitStructureAlignments(int masterMMDBID)
{
    // create or empty the Biostruc-annot-set that will contain these alignments
    // in the asn data, erasing any structure alignments currently stored there
    CBiostruc_annot_set *structureAlignments = dataManager->GetStructureAlignments();
    if (structureAlignments) {
        structureAlignments->SetId().clear();
        structureAlignments->SetDescr().clear();
        structureAlignments->SetFeatures().clear();
    } else {
        structureAlignments = new CBiostruc_annot_set();
        dataManager->SetStructureAlignments(structureAlignments);
    }

    // set up the skeleton of the new Biostruc-annot-set
    // new Mmdb-id
    structureAlignments->SetId().resize(1);
    structureAlignments->SetId().front().Reset(new CBiostruc_id());
    CMmdb_id *mid = new CMmdb_id(masterMMDBID);
    structureAlignments->SetId().front().GetObject().SetMmdb_id(*mid);
    // new Biostruc-feature-set
    CRef<CBiostruc_feature_set> featSet(new CBiostruc_feature_set());
    featSet->SetId().Set(NO_DOMAIN);
    featSet->SetFeatures();    // just create an empty list
    structureAlignments->SetFeatures().resize(1, featSet);

    // flag a change in data
    SetDataChanged(eStructureAlignmentData);
}

void StructureSet::AddStructureAlignment(CBiostruc_feature *feature,
    int masterDomainID, int dependentDomainID)
{
    CBiostruc_annot_set *structureAlignments = dataManager->GetStructureAlignments();
    if (!structureAlignments) {
        WARNINGMSG("StructureSet::AddStructureAlignment() - creating new structure alignment list");
        InitStructureAlignments(objects.front()->mmdbID);
        structureAlignments = dataManager->GetStructureAlignments();
    }

    // check master domain ID, to see if alignments have crossed master's domain boundaries
    int *currentMasterDomainID = &(structureAlignments->SetFeatures().front().GetObject().SetId().Set());
    if (*currentMasterDomainID == NO_DOMAIN)
        *currentMasterDomainID = masterDomainID;
    else if ((*currentMasterDomainID % 100) != (masterDomainID % 100))
        *currentMasterDomainID = (*currentMasterDomainID / 100) * 100;

    // check to see if this dependent domain already has an alignment; if so, increment alignment #
    CBiostruc_feature_set::TFeatures::const_iterator
        f, fe = structureAlignments->GetFeatures().front().GetObject().GetFeatures().end();
    for (f=structureAlignments->GetFeatures().front().GetObject().GetFeatures().begin(); f!=fe; ++f) {
        if ((f->GetObject().GetId().Get() / 10) == (dependentDomainID / 10))
            ++dependentDomainID;
    }
    CBiostruc_feature_id id(dependentDomainID);
    feature->SetId(id);

    CRef<CBiostruc_feature> featureRef(feature);
    structureAlignments->SetFeatures().front().GetObject().SetFeatures().resize(
        structureAlignments->GetFeatures().front().GetObject().GetFeatures().size() + 1, featureRef);

    // flag a change in data
    SetDataChanged(eStructureAlignmentData);
}

void StructureSet::RemoveStructureAlignments(void)
{
    dataManager->SetStructureAlignments(NULL);
    // flag a change in data
    SetDataChanged(eStructureAlignmentData);
}

void StructureSet::ReplaceAlignmentSet(AlignmentSet *newAlignmentSet)
{
    ASNDataManager::SeqAnnotList *seqAnnots = dataManager->GetOrCreateSequenceAlignments();
    if (!seqAnnots) {
        ERRORMSG("StructureSet::ReplaceAlignmentSet() - "
            << "can't figure out where in the asn the alignments are to go");
        return;
    }

    // update the AlignmentSet
    if (alignmentSet)
        delete alignmentSet;
    alignmentSet = newAlignmentSet;

    // update the asn alignments
    seqAnnots->resize(alignmentSet->newAsnAlignmentData->size());
    ASNDataManager::SeqAnnotList::iterator o = seqAnnots->begin();
    ASNDataManager::SeqAnnotList::iterator n, ne = alignmentSet->newAsnAlignmentData->end();
    for (n=alignmentSet->newAsnAlignmentData->begin(); n!=ne; ++n, ++o)
        o->Reset(n->GetPointer());   // copy each Seq-annot CRef

    // don't set data PSSM/row order flags here; done by AlignmentManager::SavePairwiseFromMultiple()
    SetDataChanged(eAnyAlignmentData);
}

void StructureSet::ReplaceUpdates(ncbi::objects::CCdd::TPending& newUpdates)
{
    dataManager->ReplaceUpdates(newUpdates);
}

void StructureSet::RemoveUnusedSequences(void)
{
	ASNDataManager::SequenceList updateSequences;
    if (alignmentManager) alignmentManager->GetUpdateSequences(&updateSequences);
    dataManager->RemoveUnusedSequences(alignmentSet, updateSequences);
}

bool StructureSet::MonitorAlignments(void) const
{
    return dataManager->MonitorAlignments();
}

bool StructureSet::SaveASNData(const char *filename, bool doBinary, unsigned int *changeFlags)
{
    // force a save of any edits to alignment and updates first (it's okay if this has already been done)
    GlobalMessenger()->SequenceWindowsSave(true);
    /*if (dataManager->HasDataChanged())*/ RemoveUnusedSequences();

    // create and temporarily attach a style dictionary, and annotation set + camera info
    // to the data (and then remove it again, so it's never out of date)
    CRef < CCn3d_style_dictionary > styleDictionary(styleManager->CreateASNStyleDictionary());
    dataManager->SetStyleDictionary(*styleDictionary);
    CRef < CCn3d_user_annotations > userAnnotations(new CCn3d_user_annotations());
    if (!styleManager->SaveToASNUserAnnotations(userAnnotations.GetPointer()) ||
        (objects.size() >= 1 && !renderer->SaveToASNViewSettings(userAnnotations.GetPointer()))) {
        ERRORMSG("StructureSet::SaveASNData() - error creating user annotations blob");
        return false;
    }
    if (userAnnotations->IsSetAnnotations() || userAnnotations->IsSetView())
        dataManager->SetUserAnnotations(*userAnnotations);

    string err;
    bool writeOK = MonitorAlignments();
    if (!writeOK)
        err = "MonitorAlignments() returned error, no file written";
    else
        writeOK = dataManager->WriteDataToFile(filename, doBinary, &err, eFNP_Replace);

    // remove style dictionary and annotations from asn
    dataManager->RemoveStyleDictionary();
    dataManager->RemoveUserAnnotations();

    if (writeOK) {
        *changeFlags = dataManager->GetDataChanged();
        dataManager->SetDataUnchanged();
    } else {
        ERRORMSG("Write failed: " << err);
    }
    return writeOK;
}

// because the frame map (for each frame, a list of diplay lists) is complicated
// to create, this just verifies that all display lists occur exactly once
// in the map. Also, make sure that total # display lists in all frames adds up.
void StructureSet::VerifyFrameMap(void) const
{
    TRACEMSG("# display lists: " << (lastDisplayList - OpenGLRenderer::FIRST_LIST + 1));
    for (unsigned int l=OpenGLRenderer::FIRST_LIST; l<=lastDisplayList; ++l) {
        bool found = false;
        for (unsigned int f=0; f<frameMap.size(); ++f) {
            DisplayLists::const_iterator d, de=frameMap[f].end();
            for (d=frameMap[f].begin(); d!=de; ++d) {
                if (*d == l) {
                    if (!found)
                        found = true;
                    else
                        ERRORMSG("frameMap: repeated display list " << l);
                }
            }
        }
        if (!found)
            ERRORMSG("display list " << l << " not in frameMap");
    }

    unsigned int nLists = 0;
    for (unsigned int f=0; f<frameMap.size(); ++f) {
        DisplayLists::const_iterator d, de=frameMap[f].end();
        for (d=frameMap[f].begin(); d!=de; ++d) ++nLists;
    }
    if (nLists != lastDisplayList)
        ERRORMSG("frameMap has too many display lists");
}

void StructureSet::SetCenter(const Vector *given)
{
    Vector siteSum;
    int nAtoms = 0;
    double dist;
    maxDistFromCenter = 0.0;

    // set new center if given one
    if (given) center = *given;

    // loop trough all atoms twice - once to get average center, then once to
    // find max distance from this center
    for (int i=0; i<2; ++i) {
        if (given && i==0) continue; // skip center calculation if given one
        ObjectList::const_iterator o, oe=objects.end();
        for (o=objects.begin(); o!=oe; ++o) {
            StructureObject::CoordSetList::const_iterator c, ce=(*o)->coordSets.end();
            for (c=(*o)->coordSets.begin(); c!=ce; ++c) {
                AtomSet::AtomMap::const_iterator a, ae=(*c)->atomSet->atomMap.end();
                for (a=(*c)->atomSet->atomMap.begin(); a!=ae; ++a) {
                    Vector site(a->second.front()->site);
                    if ((*o)->IsDependent() && (*o)->transformToMaster)
                        ApplyTransformation(&site, *((*o)->transformToMaster));
                    if (i==0) {
                        siteSum += site;
                        ++nAtoms;
                    } else {
                        dist = (site - center).length();
                        if (dist > maxDistFromCenter)
                            maxDistFromCenter = dist;
                    }
                }
            }
        }
        if (i==0) {
            if (nAtoms == 0) {
                center.Set(0.0, 0.0, 0.0);
                break;
            }
            center = siteSum / nAtoms;
        }
    }
    TRACEMSG("center: " << center << ", maxDistFromCenter " << maxDistFromCenter);
    rotationCenter = center;
}

void StructureSet::CenterViewOnStructure(void)
{
    if (objects.size() != 1)
        return;

    TRACEMSG("Computing structure view...");
    Vector alphaCenter;
    double alphaRadius = 0.0;
    int nAlphas = 0;

    // loop through twice - once to get average center, then once to
    // find max distance from this center
    for (int i=0; i<2; ++i) {

        // find all biopolymer alpha residues
        ChemicalGraph::MoleculeMap::const_iterator m, me = objects.front()->graph->molecules.end();
        for (m=objects.front()->graph->molecules.begin(); m!=me; ++m) {
            if (!m->second->IsBiopolymer())
                continue;
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
            for (r=m->second->residues.begin(); r!=re; ++r) {
                if (r->second->alphaID == Residue::NO_ALPHA_ID) continue;
                AtomPntr ap(m->second->id, r->first, r->second->alphaID);
                const AtomCoord* atom = objects.front()->coordSets.front()->atomSet->GetAtom(ap, true, true);
                if (atom) {
                    if (i == 0) {
                        alphaCenter += atom->site;
                        ++nAlphas;
                    } else {
                        double dist = (atom->site - alphaCenter).length();
                        if (dist > alphaRadius)
                            alphaRadius = dist;
                    }
                }
            }
        }

        if (i == 0) {
            if (nAlphas == 0)
                return;
            alphaCenter /= nAlphas;
        }
    }

    renderer->CenterView(alphaCenter, alphaRadius);
    TRACEMSG("Centered view at " << alphaCenter << " radius " << alphaRadius);
}

bool StructureSet::CenterViewOnAlignedResidues(void)
{
    const BlockMultipleAlignment *alignment = alignmentManager->GetCurrentMultipleAlignment();
    if (!alignment || !alignment->GetSequenceOfRow(0) || !alignment->GetSequenceOfRow(0))
        return false;;                     // no alignment
    const Molecule *masterMolecule = alignment->GetSequenceOfRow(0)->molecule;
    if (!masterMolecule) return false;    // no structured master
    const StructureObject *masterObject;
    if (!masterMolecule->GetParentOfType(&masterObject)) return false;

    // get coords of all aligned c-alphas
    deque < Vector > coords;
    Molecule::ResidueMap::const_iterator r, re = masterMolecule->residues.end();
    for (r=masterMolecule->residues.begin(); r!=re; ++r) {
        if (!alignment->IsAligned(0U, r->first - 1)) continue;
        if (r->second->alphaID == Residue::NO_ALPHA_ID) continue;
        AtomPntr ap(masterMolecule->id, r->first, r->second->alphaID);
        const AtomCoord* atom = masterObject->coordSets.front()->atomSet->GetAtom(ap, true, true);
        if (atom) coords.push_back(atom->site);
    }
    if (coords.size() == 0)
        return false;

    // calculate center
    unsigned int i;
    Vector alignedCenter;
    for (i=0; i<coords.size(); ++i) alignedCenter += coords[i];
    alignedCenter /= coords.size();

    // find radius
    double radius = 0.0, d;
    for (i=0; i<coords.size(); ++i) {
        d = (coords[i] - alignedCenter).length();
        if (d > radius) radius = d;
    }

    // set view
    renderer->CenterView(alignedCenter, radius);
    TRACEMSG("Centered view at " << alignedCenter << " radius " << radius);
    return true;
}

bool StructureSet::Draw(const AtomSet *atomSet) const
{
    TRACEMSG("drawing StructureSet");
    if (!styleManager->CheckGlobalStyleSettings()) return false;
    return true;
}

unsigned int StructureSet::CreateName(const Residue *residue, int atomID)
{
    ++lastAtomName;
    nameMap[lastAtomName] = make_pair(residue, atomID);
    return lastAtomName;
}

bool StructureSet::GetAtomFromName(unsigned int name, const Residue **residue, int *atomID) const
{
    NameMap::const_iterator i = nameMap.find(name);
    if (i == nameMap.end()) return false;
    *residue = i->second.first;
    *atomID = i->second.second;
	return true;
}

void StructureSet::SelectedAtom(unsigned int name, bool setCenter)
{
    const Residue *residue;
    int atomID;

    if (name == OpenGLRenderer::NO_NAME || !GetAtomFromName(name, &residue, &atomID)) {
        INFOMSG("nothing selected");
        return;
    }

    // add highlight
    const Molecule *molecule;
    if (!residue->GetParentOfType(&molecule)) return;
    GlobalMessenger()->ToggleHighlight(molecule, residue->id, true);
    wxString molresid;
    if (molecule->IsHeterogen() || molecule->IsSolvent()) {
        const StructureObject *object;
        if (molecule->GetParentOfType(&object)) {
            // assume hets/solvents are single residue
            if (object->pdbID.size() > 0)
                molresid.Printf("%s heterogen/solvent molecule %i", object->pdbID.c_str(), molecule->id);
            else
                molresid = molecule->identifier->ToString().c_str();
        }
    } else
        molresid.Printf("chain %s residue %i", molecule->identifier->ToString().c_str(), residue->id);
    INFOMSG("selected " << molresid.c_str() << " (PDB: " << residue->nameGraph << ' ' << residue->namePDB
        << ") atom " << atomID << " (PDB: " << residue->GetAtomInfo(atomID)->name << ')');

    // get coordinate of picked atom, in coordinates of master frame
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return;
    object->coordSets.front()->atomSet->SetActiveEnsemble(NULL);    // don't actually know which alternate...
	const AtomCoord *pickedAtom = object->coordSets.front()->atomSet->
        GetAtom(AtomPntr(molecule->id, residue->id, atomID));
	if (!pickedAtom) {
		WARNINGMSG("Can't get coordinates for this atom (in the first coordinate set)");
		return;
	}
	Vector pickedAtomCoord = pickedAtom->site;
    if (object->IsDependent() && object->transformToMaster)
        ApplyTransformation(&pickedAtomCoord, *(object->transformToMaster));

    // print out distance to previous picked atom
    if (havePrevPickedAtomCoord)
        INFOMSG("distance to previously selected atom: " << setprecision(3) <<
            (pickedAtomCoord - prevPickedAtomCoord).length() << setprecision(6) << " A");
    prevPickedAtomCoord = pickedAtomCoord;
    havePrevPickedAtomCoord = true;

    // if indicated, use atom site as rotation center; use coordinate from first CoordSet, default altConf
    if (setCenter) {
        INFOMSG("rotating about " << object->pdbID
            << " molecule " << molecule->id << " residue " << residue->id << ", atom " << atomID);
        rotationCenter = pickedAtomCoord;
    }
}

void StructureSet::SelectByDistance(double cutoff, unsigned int options) const
{
    StructureObject::ResidueMap residuesToHighlight;

    // add residues to highlight to master list, based on proximities within objects
    ObjectList::const_iterator o, oe = objects.end();
    for (o=objects.begin(); o!=oe; ++o)
        (*o)->SelectByDistance(cutoff, options, &residuesToHighlight);

    // now actually add highlights for new selected residues
    StructureObject::ResidueMap::const_iterator r, re = residuesToHighlight.end();
    for (r=residuesToHighlight.begin(); r!=re; ++r)
        if (!GlobalMessenger()->IsHighlighted(r->second, r->first->id))
            GlobalMessenger()->ToggleHighlight(r->second, r->first->id, false);
}

const Sequence * StructureSet::FindOrCreateSequence(ncbi::objects::CBioseq& bioseq)
{
    // see if we have this sequence already
    const Sequence *seq = sequenceSet->FindMatchingSequence(bioseq.GetId());
    if (seq)
        return seq;

    // if not, add new Sequence to SequenceSet
    SequenceSet *modifiableSet = const_cast<SequenceSet*>(sequenceSet);
    seq = new Sequence(modifiableSet, bioseq);
    if (!seq->identifier) {
        ERRORMSG("StructureSet::FindOrCreateSequence() - identifier conflict, no new sequence created");
        delete seq;
        return NULL;
    }
    modifiableSet->sequences.push_back(seq);

    // add asn sequence to asn data
    if (dataManager->GetSequences()) {
        CSeq_entry *se = new CSeq_entry();
        se->SetSeq(bioseq);
        dataManager->GetSequences()->push_back(CRef<CSeq_entry>(se));
    } else
        ERRORMSG("StructureSet::FindOrCreateSequence() - no sequence list in asn data");
    SetDataChanged(eSequenceData);

    return seq;
}

void StructureSet::RejectAndPurgeSequence(const Sequence *reject, string reason, bool purge)
{
    if (!dataManager->IsCDD() || !reject || reason.size() == 0) return;

    CReject_id *rejectID = new CReject_id();
    rejectID->SetIds() = reject->bioseqASN->GetId();    // copy Seq-id lists

    CUpdate_comment *comment = new CUpdate_comment();
    comment->SetComment(reason);
    rejectID->SetDescription().push_back(CRef < CUpdate_comment > (comment));

    dataManager->AddReject(rejectID);

    if (purge)
        alignmentManager->PurgeSequence(reject->identifier);
}

const StructureSet::RejectList * StructureSet::GetRejects(void) const
{
    return dataManager->GetRejects();
}

void StructureSet::ShowRejects(void) const
{
    const RejectList *rejects = GetRejects();
    if (!rejects) {
        INFOMSG("No rejects in this CD");
        return;
    }

    INFOMSG("Rejects:");
    RejectList::const_iterator r, re = rejects->end();
    for (r=rejects->begin(); r!=re; ++r) {
        string idstr;
        CReject_id::TIds::const_iterator i, ie = (*r)->GetIds().end();
        for (i=(*r)->GetIds().begin(); i!=ie; ++i)
            idstr += (*i)->AsFastaString() + ", ";
        INFOMSG(idstr << "Reason: " <<
            (((*r)->IsSetDescription() && (*r)->GetDescription().front()->IsComment()) ?
                (*r)->GetDescription().front()->GetComment() : string("none given")));
    }
}

bool StructureSet::ConvertMimeDataToCDD(const std::string& cddName)
{
    if (!dataManager->ConvertMimeDataToCDD(cddName))
        return false;

    // make sure all structured sequences have MMDB annot tags
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet->sequences.end();
    for (s=sequenceSet->sequences.begin(); s!=se; ++s) {
        if ((*s)->molecule) {
            if ((*s)->identifier->mmdbID == MoleculeIdentifier::VALUE_NOT_SET) {
                ERRORMSG("sequence " << (*s)->identifier->ToString()
                    << " has associated molecule but no MMDB id");
                return false;
            }
            (*s)->AddMMDBAnnotTag((*s)->identifier->mmdbID);
        }
    }

    return true;
}

// trivial methods...
bool StructureSet::IsMultiStructure(void) const { return !dataManager->IsSingleStructure(); }
bool StructureSet::HasDataChanged(void) const { return dataManager->HasDataChanged(); }
void StructureSet::SetDataChanged(unsigned int what) const { dataManager->SetDataChanged(what); }
bool StructureSet::IsCDD(void) const { return dataManager->IsCDD(); }
bool StructureSet::IsCDDInMime(void) const { return dataManager->IsCDDInMime(); }
const string& StructureSet::GetCDDName(void) const { return dataManager->GetCDDName(); }
bool StructureSet::SetCDDName(const string& name) { return dataManager->SetCDDName(name); }
const string& StructureSet::GetCDDDescription(void) const { return dataManager->GetCDDDescription(); }
bool StructureSet::SetCDDDescription(const string& descr) { return dataManager->SetCDDDescription(descr); }
bool StructureSet::GetCDDNotes(StructureSet::TextLines *lines) const { return dataManager->GetCDDNotes(lines); }
bool StructureSet::SetCDDNotes(const StructureSet::TextLines& lines) { return dataManager->SetCDDNotes(lines); }
ncbi::objects::CCdd_descr_set * StructureSet::GetCDDDescrSet(void) { return dataManager->GetCDDDescrSet(); }
ncbi::objects::CAlign_annot_set * StructureSet::GetCDDAnnotSet(void) { return dataManager->GetCDDAnnotSet(); }



///// StructureObject stuff /////

const int StructureObject::NO_MMDB_ID = -1;
const double StructureObject::NO_TEMPERATURE = kMin_Double;

StructureObject::StructureObject(StructureBase *parent, const CBiostruc& biostruc, bool master) :
    StructureBase(parent), isMaster(master), mmdbID(NO_MMDB_ID), transformToMaster(NULL),
    minTemperature(NO_TEMPERATURE), maxTemperature(NO_TEMPERATURE)
{
    // set numerical id simply based on # objects in parentSet
    id = parentSet->objects.size() + 1;

    // get MMDB id
    CBiostruc::TId::const_iterator j, je=biostruc.GetId().end();
    for (j=biostruc.GetId().begin(); j!=je; ++j) {
        if (j->GetObject().IsMmdb_id()) {
            mmdbID = j->GetObject().GetMmdb_id().Get();
            break;
        }
    }
    TRACEMSG("MMDB id " << mmdbID);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        CBiostruc::TDescr::const_iterator k, ke=biostruc.GetDescr().end();
        for (k=biostruc.GetDescr().begin(); k!=ke; ++k) {
            if (k->GetObject().IsName()) {
                pdbID = k->GetObject().GetName();
                break;
            }
        }
    }
    TRACEMSG("PDB id " << pdbID);

    // get atom and feature spatial coordinates
    if (biostruc.IsSetModel()) {
        // iterate SEQUENCE OF Biostruc-model
        CBiostruc::TModel::const_iterator i, ie=biostruc.GetModel().end();
        for (i=biostruc.GetModel().begin(); i!=ie; ++i) {

            // don't know how to deal with these...
            if (i->GetObject().GetType() == eModel_type_ncbi_vector ||
                i->GetObject().GetType() == eModel_type_other) continue;

            // otherwise, assume all models in this set are of same type
            if (i->GetObject().GetType() == eModel_type_ncbi_backbone)
                parentSet->isAlphaOnly = true;
            else
                parentSet->isAlphaOnly = false;

            // load each Biostruc-model into a CoordSet
            if (i->GetObject().IsSetModel_coordinates()) {
                CoordSet *coordSet =
                    new CoordSet(this, i->GetObject().GetModel_coordinates());
                coordSets.push_back(coordSet);
            }
        }
    }

    TRACEMSG("temperature range: " << minTemperature << " to " << maxTemperature);

    // get graph - must be done after atom coordinates are loaded, so we can
    // avoid storing graph nodes for atoms not present in the model
    graph = new ChemicalGraph(this, biostruc.GetChemical_graph(), biostruc.GetFeatures());
}

bool StructureObject::SetTransformToMaster(const CBiostruc_annot_set& annot, int masterMMDBID)
{
    CBiostruc_annot_set::TFeatures::const_iterator f1, f1e=annot.GetFeatures().end();
    for (f1=annot.GetFeatures().begin(); f1!=f1e; ++f1) {
        CBiostruc_feature_set::TFeatures::const_iterator f2, f2e=f1->GetObject().GetFeatures().end();
        for (f2=f1->GetObject().GetFeatures().begin(); f2!=f2e; ++f2) {

            // skip if already used
            if (f2->GetObject().IsSetId() &&
                    parentSet->usedFeatures.find(f2->GetObject().GetId().Get()) !=
                        parentSet->usedFeatures.end())
                continue;

            // look for alignment feature
            if (f2->GetObject().IsSetType() &&
				f2->GetObject().GetType() == CBiostruc_feature::eType_alignment &&
                f2->GetObject().IsSetLocation() &&
                f2->GetObject().GetLocation().IsAlignment()) {
                const CChem_graph_alignment& graphAlign =
					f2->GetObject().GetLocation().GetAlignment();

                // find transform alignment of this object with master
                if (graphAlign.GetDimension() == 2 &&
                    graphAlign.GetBiostruc_ids().size() == 2 &&
                    graphAlign.IsSetTransform() &&
                    graphAlign.GetTransform().size() == 1 &&
                    graphAlign.GetBiostruc_ids().front().GetObject().IsMmdb_id() &&
                    graphAlign.GetBiostruc_ids().front().GetObject().GetMmdb_id().Get() == masterMMDBID &&
                    graphAlign.GetBiostruc_ids().back().GetObject().IsMmdb_id() &&
                    graphAlign.GetBiostruc_ids().back().GetObject().GetMmdb_id().Get() == mmdbID) {

                    // mark feature as used
                    if (f2->GetObject().IsSetId())
                        parentSet->usedFeatures[f2->GetObject().GetId().Get()] = true;
                    TRACEMSG("got transform for " << pdbID << "->master");

                    // unpack transform into matrix, moves in reverse order;
                    Matrix xform;
                    transformToMaster = new Matrix();
                    CTransform::TMoves::const_iterator
                        m, me=graphAlign.GetTransform().front().GetObject().GetMoves().end();
                    for (m=graphAlign.GetTransform().front().GetObject().GetMoves().begin(); m!=me; ++m) {
                        Matrix xmat;
                        double scale;
                        if (m->GetObject().IsTranslate()) {
                            const CTrans_matrix& trans = m->GetObject().GetTranslate();
                            scale = 1.0 / trans.GetScale_factor();
                            SetTranslationMatrix(&xmat,
                                Vector(scale * trans.GetTran_1(),
                                       scale * trans.GetTran_2(),
                                       scale * trans.GetTran_3()));
                        } else { // rotate
                            const CRot_matrix& rot = m->GetObject().GetRotate();
                            scale = 1.0 / rot.GetScale_factor();
                            xmat.m[0]=scale*rot.GetRot_11(); xmat.m[1]=scale*rot.GetRot_21(); xmat.m[2]= scale*rot.GetRot_31(); xmat.m[3]=0;
                            xmat.m[4]=scale*rot.GetRot_12(); xmat.m[5]=scale*rot.GetRot_22(); xmat.m[6]= scale*rot.GetRot_32(); xmat.m[7]=0;
                            xmat.m[8]=scale*rot.GetRot_13(); xmat.m[9]=scale*rot.GetRot_23(); xmat.m[10]=scale*rot.GetRot_33(); xmat.m[11]=0;
                            xmat.m[12]=0;                    xmat.m[13]=0;                    xmat.m[14]=0;                     xmat.m[15]=1;
                        }
                        ComposeInto(transformToMaster, xmat, xform);
                        xform = *transformToMaster;
                    }
                    return true;
                }
            }
        }
    }

    WARNINGMSG("Can't get structure alignment for dependent " << pdbID
        << " with master " << masterMMDBID << ";\nwill likely require manual realignment");
    return false;
}

static void AddDomain(int *domain, const Molecule *molecule, const Block::Range *range)
{
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return;
    for (int l=range->from; l<=range->to; ++l) {
        if (molecule->residueDomains[l] != Molecule::NO_DOMAIN_SET) {
            if (*domain == NO_DOMAIN)
                *domain = object->domainID2MMDB.find(molecule->residueDomains[l])->second;
            else if (*domain != MULTI_DOMAIN &&
                     *domain != object->domainID2MMDB.find(molecule->residueDomains[l])->second)
                *domain = MULTI_DOMAIN;
        }
    }
}

void StructureObject::RealignStructure(int nCoords,
    const Vector * const *masterCoords, const Vector * const *dependentCoords,
    const double *weights, int dependentRow)
{
    Vector masterCOM, dependentCOM; // centers of mass for master, dependent
    Matrix dependentRotation;       // rotation to align dependent with master
    if (!transformToMaster) transformToMaster = new Matrix();

    // do the fit
    RigidBodyFit(nCoords, masterCoords, dependentCoords, weights, masterCOM, dependentCOM, dependentRotation);

    // apply the resulting transform elements from the fit to this object's transform Matrix
    Matrix single, combined;
    SetTranslationMatrix(&single, -dependentCOM);
    ComposeInto(transformToMaster, dependentRotation, single);
    combined = *transformToMaster;
    SetTranslationMatrix(&single, masterCOM);
    ComposeInto(transformToMaster, single, combined);

    // print out RMSD
    INFOMSG("RMSD of alpha coordinates used to align master structure and " << pdbID << ": "
        << setprecision(3) << ComputeRMSD(nCoords, masterCoords, dependentCoords, transformToMaster) << setprecision(6) << " A");

    // create a new Biostruc-feature that contains this alignment
    CBiostruc_feature *feature = new CBiostruc_feature();
    feature->SetType(CBiostruc_feature::eType_alignment);
    CRef<CBiostruc_feature::C_Location> location(new CBiostruc_feature::C_Location());
    feature->SetLocation(*location);
    CChem_graph_alignment *graphAlignment = new CChem_graph_alignment();
    location.GetObject().SetAlignment(*graphAlignment);

    // fill out the Chem-graph-alignment
    graphAlignment->SetDimension(2);
    CMmdb_id
        *masterMID = new CMmdb_id(parentSet->objects.front()->mmdbID),
        *dependentMID = new CMmdb_id(mmdbID);
    CBiostruc_id
        *masterBID = new CBiostruc_id(),
        *dependentBID = new CBiostruc_id();
    masterBID->SetMmdb_id(*masterMID);
    dependentBID->SetMmdb_id(*dependentMID);
    graphAlignment->SetBiostruc_ids().resize(2);
    graphAlignment->SetBiostruc_ids().front().Reset(masterBID);
    graphAlignment->SetBiostruc_ids().back().Reset(dependentBID);
    graphAlignment->SetAlignment().resize(2);

    // fill out sequence alignment intervals, tracking domains in alignment
    const BlockMultipleAlignment *multiple = parentSet->alignmentManager->GetCurrentMultipleAlignment();
    int masterDomain = NO_DOMAIN, dependentDomain = NO_DOMAIN;
    const Molecule
        *masterMolecule = multiple->GetSequenceOfRow(0)->molecule,
        *dependentMolecule = multiple->GetSequenceOfRow(dependentRow)->molecule;
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() > 0) {
        CChem_graph_pntrs
            *masterCGPs = new CChem_graph_pntrs(),
            *dependentCGPs = new CChem_graph_pntrs();
        graphAlignment->SetAlignment().front().Reset(masterCGPs);
        graphAlignment->SetAlignment().back().Reset(dependentCGPs);
        CResidue_pntrs
            *masterRPs = new CResidue_pntrs(),
            *dependentRPs = new CResidue_pntrs();
        masterCGPs->SetResidues(*masterRPs);
        dependentCGPs->SetResidues(*dependentRPs);

        masterRPs->SetInterval().resize(blocks.size());
        dependentRPs->SetInterval().resize(blocks.size());
        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
        CResidue_pntrs::TInterval::iterator
            mi = masterRPs->SetInterval().begin(),
            si = dependentRPs->SetInterval().begin();
        for (b=blocks.begin(); b!=be; ++b, ++mi, ++si) {
            CResidue_interval_pntr
                *masterRIP = new CResidue_interval_pntr(),
                *dependentRIP = new CResidue_interval_pntr();
            mi->Reset(masterRIP);
            si->Reset(dependentRIP);

            masterRIP->SetMolecule_id().Set(masterMolecule->id);
            dependentRIP->SetMolecule_id().Set(dependentMolecule->id);

            const Block::Range *range = (*b)->GetRangeOfRow(0);
            masterRIP->SetFrom().Set(range->from + 1); // +1 to convert seqLoc to residueID
            masterRIP->SetTo().Set(range->to + 1);
            AddDomain(&masterDomain, masterMolecule, range);

            range = (*b)->GetRangeOfRow(dependentRow);
            dependentRIP->SetFrom().Set(range->from + 1);
            dependentRIP->SetTo().Set(range->to + 1);
            AddDomain(&dependentDomain, dependentMolecule, range);
        }
    }

    // fill out structure alignment transform
    CTransform *xform = new CTransform();
    graphAlignment->SetTransform().resize(1);
    graphAlignment->SetTransform().front().Reset(xform);
    xform->SetId(1);
    xform->SetMoves().resize(3);
    CTransform::TMoves::iterator m = xform->SetMoves().begin();
    for (int i=0; i<3; ++i, ++m) {
        CMove *move = new CMove();
        m->Reset(move);
        static const int scaleFactor = 100000;
        if (i == 0) {   // translate dependent so its COM is at origin
            CTrans_matrix *trans = new CTrans_matrix();
            move->SetTranslate(*trans);
            trans->SetScale_factor(scaleFactor);
            trans->SetTran_1((int)(-(dependentCOM.x * scaleFactor)));
            trans->SetTran_2((int)(-(dependentCOM.y * scaleFactor)));
            trans->SetTran_3((int)(-(dependentCOM.z * scaleFactor)));
        } else if (i == 1) {
            CRot_matrix *rot = new CRot_matrix();
            move->SetRotate(*rot);
            rot->SetScale_factor(scaleFactor);
            rot->SetRot_11((int)(dependentRotation[0] * scaleFactor));
            rot->SetRot_12((int)(dependentRotation[4] * scaleFactor));
            rot->SetRot_13((int)(dependentRotation[8] * scaleFactor));
            rot->SetRot_21((int)(dependentRotation[1] * scaleFactor));
            rot->SetRot_22((int)(dependentRotation[5] * scaleFactor));
            rot->SetRot_23((int)(dependentRotation[9] * scaleFactor));
            rot->SetRot_31((int)(dependentRotation[2] * scaleFactor));
            rot->SetRot_32((int)(dependentRotation[6] * scaleFactor));
            rot->SetRot_33((int)(dependentRotation[10] * scaleFactor));
        } else if (i == 2) {    // translate dependent so its COM is at COM of master
            CTrans_matrix *trans = new CTrans_matrix();
            move->SetTranslate(*trans);
            trans->SetScale_factor(scaleFactor);
            trans->SetTran_1((int)(masterCOM.x * scaleFactor));
            trans->SetTran_2((int)(masterCOM.y * scaleFactor));
            trans->SetTran_3((int)(masterCOM.z * scaleFactor));
        }
    }

    // store the new alignment in the Biostruc-annot-set,
    // setting the feature id depending on the aligned domain(s)
    if (masterDomain == NO_DOMAIN) masterDomain = 0;    // can happen if single domain chain
    if (dependentDomain == NO_DOMAIN) dependentDomain = 0;
    const StructureObject *masterObject;
    if (!masterMolecule->GetParentOfType(&masterObject)) return;
    int
        masterDomainID = masterObject->mmdbID*10000 + masterMolecule->id*100 + masterDomain,
        dependentDomainID = mmdbID*100000 + dependentMolecule->id*1000 + dependentDomain*10 + 1;
    parentSet->AddStructureAlignment(feature, masterDomainID, dependentDomainID);

    // for backward-compatibility with Cn3D 3.5, need name to encode chain/domain
    CNcbiOstrstream oss;
    oss << masterMolecule->identifier->pdbID << ((char) masterMolecule->identifier->pdbChain) << masterDomain << ' '
        << dependentMolecule->identifier->pdbID << ((char) dependentMolecule->identifier->pdbChain) << dependentDomain << ' '
        << "Structure alignment of dependent " << multiple->GetSequenceOfRow(dependentRow)->identifier->ToString()
        << " with master " << multiple->GetSequenceOfRow(0)->identifier->ToString()
        << ", as computed by Cn3D";
    feature->SetName((string) CNcbiOstrstreamToString(oss));
}

void StructureObject::SelectByDistance(double cutoff, unsigned int options,
    ResidueMap *selectedResidues) const
{
    // first make a list of coordinates of atoms in selected residues,
    typedef vector < const AtomCoord * > CoordList;
    CoordList highlightedAtoms;
    typedef map < const Molecule * , bool > MoleculeList;
    MoleculeList moleculesWithHighlights;

    ChemicalGraph::MoleculeMap::const_iterator m, me = graph->molecules.end();
    for (m=graph->molecules.begin(); m!=me; ++m) {
        Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
        for (r=m->second->residues.begin(); r!=re; ++r) {

            if (GlobalMessenger()->IsHighlighted(m->second, r->second->id)) {
                const Residue::AtomInfoMap& atomInfos = r->second->GetAtomInfos();
                Residue::AtomInfoMap::const_iterator a, ae = atomInfos.end();
                for (a=atomInfos.begin(); a!=ae; ++a) {
                    const AtomCoord *atomCoord = coordSets.front()->atomSet->
                        GetAtom(AtomPntr(m->second->id, r->second->id, a->first), true, true);
                    if (atomCoord) highlightedAtoms.push_back(atomCoord);
                }
                moleculesWithHighlights[m->second] = true;
            }
        }
    }
    if (highlightedAtoms.size() == 0) return;

    // now make a list of unselected residues to check for proximity
    typedef vector < const Residue * > ResidueList;
    ResidueList unhighlightedResidues;

    for (m=graph->molecules.begin(); m!=me; ++m) {
        Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();

        if ((options & StructureSet::eSelectOtherMoleculesOnly) &&
                moleculesWithHighlights.find(m->second) != moleculesWithHighlights.end())
            continue;

        for (r=m->second->residues.begin(); r!=re; ++r) {

            if (!GlobalMessenger()->IsHighlighted(m->second, r->second->id) &&
                    (((options & StructureSet::eSelectProtein) && m->second->IsProtein()) ||
                     ((options & StructureSet::eSelectNucleotide) && m->second->IsNucleotide()) ||
                     ((options & StructureSet::eSelectHeterogen) && m->second->IsHeterogen()) ||
                     ((options & StructureSet::eSelectSolvent) && m->second->IsSolvent())))
                unhighlightedResidues.push_back(r->second);
        }
    }
    if (unhighlightedResidues.size() == 0) return;

    // now check all unhighlighted residues, to see if any atoms are within cutoff distance
    // of any highlighted atoms; if so, add to residue selection list
    CoordList::const_iterator h, he = highlightedAtoms.end();
    ResidueList::const_iterator u, ue = unhighlightedResidues.end();
    for (u=unhighlightedResidues.begin(); u!=ue; ++u) {
        const Molecule *molecule;
        if (!(*u)->GetParentOfType(&molecule)) continue;

        const Residue::AtomInfoMap& atomInfos = (*u)->GetAtomInfos();
        Residue::AtomInfoMap::const_iterator a, ae = atomInfos.end();
        for (a=atomInfos.begin(); a!=ae; ++a) {
            const AtomCoord *uAtomCoord = coordSets.front()->atomSet->
                GetAtom(AtomPntr(molecule->id, (*u)->id, a->first), true, true);
            if (!uAtomCoord) continue;

            // for each unhighlighted atom, check for proximity to a highlighted atom
            for (h=highlightedAtoms.begin(); h!=he; ++h) {
                if ((uAtomCoord->site - (*h)->site).length() <= cutoff)
                    break;
            }
            if (h != he)
                break;
        }

        // if any atom of this residue is near a highlighted atom, add to selection list
        if (a != ae)
            (*selectedResidues)[*u] = molecule;
    }
}

END_SCOPE(Cn3D)
