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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.96  2002/02/19 14:59:40  thiessen
* add CDD reject and purge sequence
*
* Revision 1.95  2002/02/12 17:19:22  thiessen
* first working structure import
*
* Revision 1.94  2002/02/05 18:53:26  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.93  2002/01/19 02:34:50  thiessen
* fixes for changes in asn serialization API
*
* Revision 1.92  2002/01/03 16:18:40  thiessen
* add distance selection
*
* Revision 1.91  2001/12/21 23:08:48  thiessen
* add residue info when structure selected
*
* Revision 1.90  2001/12/15 03:15:59  thiessen
* adjustments for slightly changed object loader Set...() API
*
* Revision 1.89  2001/12/12 14:04:15  thiessen
* add missing object headers after object loader change
*
* Revision 1.88  2001/12/06 23:13:46  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.87  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.86  2001/11/27 16:26:09  thiessen
* major update to data management system
*
* Revision 1.85  2001/10/30 02:54:13  thiessen
* add Biostruc cache
*
* Revision 1.84  2001/10/17 17:46:22  thiessen
* save camera setup and rotation center in files
*
* Revision 1.83  2001/10/09 18:57:05  thiessen
* add CDD references editing dialog
*
* Revision 1.82  2001/10/08 00:00:09  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.81  2001/10/01 16:04:25  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.80  2001/09/27 20:58:28  thiessen
* add VisibleString filter option
*
* Revision 1.79  2001/09/27 15:37:59  thiessen
* decouple sequence import and BLAST
*
* Revision 1.78  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.77  2001/09/18 03:10:45  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.76  2001/08/27 00:06:23  thiessen
* add structure evidence to CDD annotation
*
* Revision 1.75  2001/08/13 22:30:59  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.74  2001/08/10 15:01:57  thiessen
* fill out shortcuts; add update show/hide menu
*
* Revision 1.73  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.72  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.71  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.70  2001/07/10 16:39:55  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.69  2001/07/04 19:39:17  thiessen
* finish user annotation system
*
* Revision 1.68  2001/06/29 18:13:58  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.67  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.66  2001/06/15 14:06:40  thiessen
* save/load asn styles now complete
*
* Revision 1.65  2001/06/14 17:45:10  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.64  2001/06/14 00:34:01  thiessen
* asn additions
*
* Revision 1.63  2001/06/07 19:05:38  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.62  2001/06/05 13:21:08  thiessen
* fix structure alignment list problems
*
* Revision 1.61  2001/05/31 18:47:09  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.60  2001/05/17 18:32:47  thiessen
* fix sequence/molecule/master match bug
*
* Revision 1.59  2001/05/14 16:04:32  thiessen
* fix minor row reordering bug
*
* Revision 1.58  2001/05/11 18:20:11  thiessen
* fix loading of cdd with repeated master structure
*
* Revision 1.57  2001/05/02 13:46:28  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.56  2001/04/17 20:15:39  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.55  2001/03/23 15:14:07  thiessen
* load sidechains in CDD's
*
* Revision 1.54  2001/03/23 04:18:52  thiessen
* parse and display disulfides
*
* Revision 1.53  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.52  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.51  2001/02/22 00:30:07  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.50  2001/02/16 00:40:01  thiessen
* remove unused sequences from asn data
*
* Revision 1.49  2001/02/13 01:03:57  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.48  2001/02/08 23:01:51  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.47  2001/02/02 20:17:33  thiessen
* can read in CDD with multi-structure but no struct. alignments
*
* Revision 1.46  2001/01/30 20:51:19  thiessen
* minor fixes
*
* Revision 1.45  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.44  2001/01/18 19:37:28  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.43  2001/01/12 01:34:18  thiessen
* network Biostruc load
*
* Revision 1.42  2000/12/29 19:23:40  thiessen
* save row order
*
* Revision 1.41  2000/12/22 19:26:40  thiessen
* write cdd output files
*
* Revision 1.40  2000/12/21 23:42:16  thiessen
* load structures from cdd's
*
* Revision 1.39  2000/12/20 23:47:48  thiessen
* load CDD's
*
* Revision 1.38  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.37  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.36  2000/11/30 15:49:40  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.35  2000/11/13 18:06:53  thiessen
* working structure re-superpositioning
*
* Revision 1.34  2000/11/12 04:03:00  thiessen
* working file save including alignment edits
*
* Revision 1.33  2000/11/11 21:15:55  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.32  2000/11/02 16:56:03  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.31  2000/09/20 22:22:29  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.30  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.29  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.28  2000/09/11 14:06:29  thiessen
* working alignment coloring
*
* Revision 1.27  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.26  2000/08/30 19:48:43  thiessen
* working sequence window
*
* Revision 1.25  2000/08/29 04:34:27  thiessen
* working alignment manager, IBM
*
* Revision 1.24  2000/08/28 23:47:19  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.23  2000/08/28 18:52:42  thiessen
* start unpacking alignments
*
* Revision 1.22  2000/08/27 18:52:22  thiessen
* extract sequence information
*
* Revision 1.21  2000/08/21 19:31:48  thiessen
* add style consistency checking
*
* Revision 1.20  2000/08/21 17:22:38  thiessen
* add primitive highlighting for testing
*
* Revision 1.19  2000/08/17 14:24:06  thiessen
* added working StyleManager
*
* Revision 1.18  2000/08/13 02:43:01  thiessen
* added helix and strand objects
*
* Revision 1.17  2000/08/07 14:13:16  thiessen
* added animation frames
*
* Revision 1.16  2000/08/07 00:21:18  thiessen
* add display list mechanism
*
* Revision 1.15  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.14  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.13  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.12  2000/07/18 16:50:11  thiessen
* more friendly rotation center setting
*
* Revision 1.11  2000/07/18 00:06:00  thiessen
* allow arbitrary rotation center
*
* Revision 1.10  2000/07/17 22:37:18  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.9  2000/07/17 04:20:50  thiessen
* now does correct structure alignment transformation
*
* Revision 1.8  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.7  2000/07/12 02:00:15  thiessen
* add basic wxWindows GUI
*
* Revision 1.6  2000/07/11 13:45:31  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.5  2000/07/01 15:43:50  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.4  2000/06/29 16:46:07  thiessen
* use NCBI streams correctly
*
* Revision 1.3  2000/06/29 14:35:06  thiessen
* new atom_set files
*
* Revision 1.2  2000/06/28 13:07:55  thiessen
* store alt conf ensembles
*
* Revision 1.1  2000/06/27 20:09:40  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>

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

#include "cn3d/structure_set.hpp"
#include "cn3d/data_manager.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/show_hide_manager.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/alignment_set.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cn3d_cache.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"

#include <objseq.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

StructureSet::StructureSet(CNcbi_mime_asn1 *mime, int structureLimit, OpenGLRenderer *r) :
    StructureBase(NULL), renderer(r)
{
    dataManager = new ASNDataManager(mime);
    Load(structureLimit);
}

StructureSet::StructureSet(CCdd *cdd, int structureLimit, OpenGLRenderer *r) :
    StructureBase(NULL), renderer(r)
{
    dataManager = new ASNDataManager(cdd);
    Load(structureLimit);
}

void StructureSet::LoadSequencesForSingleStructure(void)
{
    sequenceSet = new SequenceSet(this, *(dataManager->GetSequences()));

    if (objects.size() > 1)
        ERR_POST(Error << "LoadSequencesForSingleStructure() called, but there is > 1 structure");
    if (objects.size() != 1) return;

    // look for biopolymer molecules
    ChemicalGraph::MoleculeMap::const_iterator m, me = objects.front()->graph->molecules.end();
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet->sequences.end();
    for (m=objects.front()->graph->molecules.begin(); m!=me; m++) {
        if (!m->second->IsProtein() && !m->second->IsNucleotide()) continue;

        // find matching sequence for each biopolymer
        for (s=sequenceSet->sequences.begin(); s!=se; s++) {
            if ((*s)->molecule != NULL) continue; // skip already-matched sequences

            if (m->second->identifier == (*s)->identifier) {

                // verify length
                if (m->second->residues.size() != (*s)->Length()) {
                    ERR_POST(Error
                        << "LoadSequencesForSingleStructure() - length mismatch between sequence gi "
                        << "and matching molecule " << m->second->identifier->ToString());
                    continue;
                }
                TESTMSG("matched sequence " << " gi " << (*s)->identifier->gi << " with object "
                    << objects.front()->pdbID << " moleculeID " << m->second->id);

                (const_cast<Molecule*>(m->second))->sequence = *s;
                (const_cast<Sequence*>(*s))->molecule = m->second;
                break;
            }
        }
        if (s == se)
            ERR_POST(Error << "LoadSequencesForSingleStructure() - can't find sequence for molecule "
                << m->second->identifier->ToString());
    }
}

bool StructureSet::LoadMaster(int masterMMDBID)
{
    if (objects.size() > 0) return false;
    TESTMSG("loading master " << masterMMDBID);

    if (dataManager->GetMasterStructure()) {
        objects.push_back(new StructureObject(this, *(dataManager->GetMasterStructure()), true));
        if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && objects.front()->mmdbID != masterMMDBID)
            ERR_POST(Error << "StructureSet::LoadMaster() - mismatched master MMDB ID");
    } else if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && dataManager->GetStructureList()) {
        ASNDataManager::BiostrucList::const_iterator b, be = dataManager->GetStructureList()->end();
        for (b=dataManager->GetStructureList()->begin(); b!=be; b++) {
            if ((*b)->GetId().front()->IsMmdb_id() &&
                (*b)->GetId().front()->GetMmdb_id().Get() == masterMMDBID) {
                objects.push_back(new StructureObject(this, **b, true));
                usedStructures[b->GetPointer()] = true;
                break;
            }
        }
    }
    if (masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET && objects.size() == 0) {
        CBiostruc biostruc;
        if (LoadBiostrucViaCache(masterMMDBID, eModel_type_ncbi_all_atom, &biostruc))
            objects.push_back(new StructureObject(this, biostruc, true));
    }
    return (objects.size() > 0);
}

bool StructureSet::MatchSequenceToMoleculeInObject(const Sequence *seq,
    const StructureObject *obj, const Sequence **seqHandle)
{
    ChemicalGraph::MoleculeMap::const_iterator m, me = obj->graph->molecules.end();
    for (m=obj->graph->molecules.begin(); m!=me; m++) {
        if (!(m->second->IsProtein() || m->second->IsNucleotide())) continue;

        if (m->second->identifier == seq->identifier) {

            // verify length
            if (m->second->residues.size() != seq->Length()) {
                ERR_POST(Error
                    << "MatchSequenceToMoleculeInObject() - length mismatch between sequence gi "
                    << "and matching molecule " << m->second->identifier->ToString());
                continue;
            }
            TESTMSG("matched sequence " << " gi " << seq->identifier->gi << " with object "
                << obj->pdbID << " moleculeID " << m->second->id);

            // sanity check
            if (m->second->sequence) {
                ERR_POST(Error << "Molecule " << m->second->identifier->ToString()
                    << " already has an associated sequence");
                continue;
            }

            // automatically duplicate Sequence if it's already associated with a molecule
            if (seq->molecule) {
                TESTMSG("duplicating sequence " << seq->identifier->ToString());
				SequenceSet *seqSetMod = const_cast<SequenceSet*>(sequenceSet);
                seq = new Sequence(seqSetMod, *(seq->bioseqASN));
                seqSetMod->sequences.push_back(seq);
                // update Sequence handle, which should be a handle to a MasterSlaveAlignment slave,
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

void StructureSet::LoadAlignmentsAndStructures(int structureLimit)
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
    typedef std::list < CRef < CSeq_align > > SeqAlignList;
    const SeqAlignList& seqAligns = alignments->front()->GetData().GetAlign();

    // we need to determine the identity of the master sequence; most rigorous way is to look
    // for a Seq-id that is present in all pairwise alignments
    const Sequence *seq1 = NULL, *seq2 = NULL, *master = NULL;
    bool seq1PresentInAll = true, seq2PresentInAll = true;

    // first, find sequences for first pairwise alignment
    typedef std::list < CRef < CSeq_id > > SeqIdList;
    const SeqIdList& firstSids = seqAligns.front()->GetSegs().IsDendiag() ?
        seqAligns.front()->GetSegs().GetDendiag().front()->GetIds() :
        seqAligns.front()->GetSegs().GetDenseg().GetIds();
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet->sequences.end();
    for (s=sequenceSet->sequences.begin(); s!=se; s++) {
        if ((*s)->identifier->MatchesSeqId(firstSids.front().GetObject())) seq1 = *s;
        if ((*s)->identifier->MatchesSeqId(firstSids.back().GetObject())) seq2 = *s;
        if (seq1 && seq2) break;
    }
    if (!(seq1 && seq2)) {
        ERR_POST(Error << "Can't match first pair of Seq-ids to Sequences");
        return;
    }

    // now, make sure one of these sequences is present in all the other pairwise alignments
    SeqAlignList::const_iterator a = seqAligns.begin(), ae = seqAligns.end();
    for (a++; a!=ae; a++) {
        const SeqIdList& sids = (*a)->GetSegs().IsDendiag() ?
            (*a)->GetSegs().GetDendiag().front()->GetIds() : (*a)->GetSegs().GetDenseg().GetIds();
        if (!seq1->identifier->MatchesSeqId(*(sids.front())) && !seq1->identifier->MatchesSeqId(*(sids.back())))
            seq1PresentInAll = false;
        if (!seq2->identifier->MatchesSeqId(*(sids.front())) && !seq2->identifier->MatchesSeqId(*(sids.back())))
            seq2PresentInAll = false;
    }
    if (!seq1PresentInAll && !seq2PresentInAll) {
        ERR_POST(Error << "All pairwise sequence alignments must have common master sequence");
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
            ERR_POST(Error << "Unable to match CDD's master3d with either sequence in first pairwise alignment");
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
                ERR_POST(Error << "Structure master does not contain either sequence in first pairwise alignment");
                return;
            }
        }
    }

    // if still ambiguous, just use the first one
    if (!master) {
        ERR_POST(Error << "Ambiguous master; using " << seq1->identifier->ToString());
        master = seq1;
    }

    TESTMSG("determined that master sequence is " << master->identifier->ToString());

    // load alignments now that we know the identity of the master
    alignmentSet = new AlignmentSet(this, master, *(dataManager->GetSequenceAlignments()));

    // check mmdb id's and load master if not already present (and if master has structure)
    if (masterMMDBID == MoleculeIdentifier::VALUE_NOT_SET) {
        masterMMDBID = master->identifier->mmdbID;
    } else if (master->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
               master->identifier->mmdbID != masterMMDBID) {
        ERR_POST(Error << "master structure (" << masterMMDBID <<
            ") disagrees with master sequence (apparently from " << master->identifier->mmdbID << ')');
        return;
    }
    if (objects.size() == 0 && structureLimit > 0 && masterMMDBID != MoleculeIdentifier::VALUE_NOT_SET)
        LoadMaster(masterMMDBID);

    // cross-match master sequence and structure
    if (objects.size() == 1 && !MatchSequenceToMoleculeInObject(master, objects.front())) {
        ERR_POST(Error << "MatchSequenceToMoleculeInObject() - can't find molecule in object "
            << objects.front()->pdbID << " to match master sequence "
            << master->identifier->ToString());
        return;
    }

    // IFF there's a master structure, then also load slave structures and cross-match sequences
    if (objects.size() == 1 && structureLimit > 1) {
        ASNDataManager::BiostrucList::const_iterator b, be;
        if (dataManager->GetStructureList()) be = dataManager->GetStructureList()->end();
        int row;
        std::vector < bool > loadedStructureForSlaveRow(alignmentSet->alignments.size(), false);

        // first, load each remaining slave structure, and for each one, find the first slave
        // sequence that matches it (and that doesn't already have structure)
        AlignmentSet::AlignmentList::const_iterator l, le = alignmentSet->alignments.end();
        if (dataManager->GetStructureList()) {
            for (b=dataManager->GetStructureList()->begin(); b!=be && objects.size()<structureLimit; b++) {

                // load structure
                if (usedStructures.find(b->GetPointer()) != usedStructures.end()) continue;
                StructureObject *object = new StructureObject(this, **b, false);
                objects.push_back(object);
                if (dataManager->GetStructureAlignments())
                    object->SetTransformToMaster(
                        *(dataManager->GetStructureAlignments()), master->identifier->mmdbID);
                usedStructures[b->GetPointer()] = true;

                // find matching unstructured slave sequence
                for (l=alignmentSet->alignments.begin(), row=0; l!=le; l++, row++) {
                    if (loadedStructureForSlaveRow[row]) continue;
                    if (MatchSequenceToMoleculeInObject((*l)->slave, object,
                            &((const_cast<MasterSlaveAlignment*>(*l))->slave))) {
                        loadedStructureForSlaveRow[row] = true;
                        break;
                    }
                }
                if (l == le)
                    ERR_POST(Error << "Warning: Structure " << object->pdbID
                        << " doesn't have a matching slave sequence in the multiple alignment");
            }
        }

        // now loop through slave rows of the alignment; if the slave
        // sequence has an MMDB ID but no structure yet, then load it.
        if (objects.size() < structureLimit && (dataManager->IsCDD() || dataManager->IsGeneralMime())) {
            for (l=alignmentSet->alignments.begin(), row=0; l!=le && objects.size()<structureLimit; l++, row++) {

                if ((*l)->slave->identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
                    !loadedStructureForSlaveRow[row]) {

                    // first check the biostruc list to see if this structure is present already
                    CRef < CBiostruc > biostruc;
                    if (dataManager->GetStructureList()) {
                        for (b=dataManager->GetStructureList()->begin(); b!=be ; b++) {
                            if ((*b)->GetId().front()->IsMmdb_id() &&
                                (*b)->GetId().front()->GetMmdb_id().Get() == (*l)->slave->identifier->mmdbID) {
                                biostruc = *b;
                                break;
                            }
                        }
                    }

                    // if not in list, load Biostruc via HTTP/cache
                    if (biostruc.Empty()) {
                        biostruc.Reset(new CBiostruc());
                        if (!LoadBiostrucViaCache((*l)->slave->identifier->mmdbID,
                                eModel_type_ncbi_all_atom, biostruc.GetPointer())) {
                            ERR_POST(Error << "Failed to load MMDB #" << (*l)->slave->identifier->mmdbID);
                            continue;
                        }
                    }

                    // create StructureObject and cross-match
                    StructureObject *object = new StructureObject(this, *biostruc, false);
                    objects.push_back(object);
                    if (dataManager->GetStructureAlignments())
                        object->SetTransformToMaster(
                            *(dataManager->GetStructureAlignments()), master->identifier->mmdbID);
                    if (!MatchSequenceToMoleculeInObject((*l)->slave, object,
                            &((const_cast<MasterSlaveAlignment*>(*l))->slave)))
                        ERR_POST(Error << "Failed to match any molecule in structure " << object->pdbID
                            << " with sequence " << (*l)->slave->identifier->ToString());
                    loadedStructureForSlaveRow[row] = true;
                }
            }
        }
    }
}

void StructureSet::Load(int structureLimit)
{
    // member data initialization
    lastAtomName = lastAtomSelected = OpenGLRenderer::NO_NAME;
    lastDisplayList = OpenGLRenderer::NO_LIST;
    sequenceSet = NULL;
    alignmentSet = NULL;
    alignmentManager = NULL;
    nDomains = 0;
    parentSet = this;
    showHideManager = new ShowHideManager();
    styleManager = new StyleManager(this);

    // if this is a single structure, then there should be one sequence per biopolymer
    if (dataManager->IsSingleStructure()) {
        const CBiostruc *masterBiostruc = dataManager->GetMasterStructure();
        if (!masterBiostruc && dataManager->GetStructureList() && dataManager->GetStructureList()->size() == 1)
            masterBiostruc = dataManager->GetStructureList()->front().GetPointer();
        if (masterBiostruc && structureLimit > 0)
            objects.push_back(new StructureObject(this, *masterBiostruc, true));
        if (dataManager->GetSequences())
            LoadSequencesForSingleStructure();
    }

    // multiple structure: should have exactly one sequence per structure (plus unstructured sequences)
    else {
        if (!dataManager->GetSequences() || !dataManager->GetSequenceAlignments()) {
            ERR_POST(Error << "Data interpreted as multiple alignment, "
                "but missing sequences and/or sequence alignments");
            return;
        }
        sequenceSet = new SequenceSet(this, *(dataManager->GetSequences()));
        LoadAlignmentsAndStructures(structureLimit);
    }

    // create alignment manager
    if (sequenceSet) {
        if (dataManager->GetUpdates())
            // if updates present, alignment manager will load those into update viewer
            alignmentManager = new AlignmentManager(sequenceSet, alignmentSet, *(dataManager->GetUpdates()));
        else
            alignmentManager = new AlignmentManager(sequenceSet, alignmentSet);
    }

#ifdef _DEBUG
    VerifyFrameMap();
#endif

    // setup show/hide items
    showHideManager->ConstructShowHideArray(this);

    // set default rendering style
    if (alignmentSet) {
        styleManager->SetGlobalRenderingStyle(StyleSettings::eTubeShortcut);
        styleManager->SetGlobalColorScheme(StyleSettings::eAlignedShortcut);
    } else {
        styleManager->SetGlobalRenderingStyle(StyleSettings::eWormShortcut);
        styleManager->SetGlobalColorScheme(StyleSettings::eSecondaryStructureShortcut);
    }

    // load user styles and annotations
    const CCn3d_style_dictionary *styles = dataManager->GetStyleDictionary();
    if (styles) {
        if (!styleManager->LoadFromASNStyleDictionary(*styles) ||
            !styleManager->CheckGlobalStyleSettings())
            ERR_POST(Error << "Error loading style dictionary");
        dataManager->RemoveStyleDictionary();   // remove now; recreated with current settings upon save
    }
    const CCn3d_user_annotations *annots = dataManager->GetUserAnnotations();
    if (annots) {
        if (!styleManager->LoadFromASNUserAnnotations(*annots) ||
            !renderer->LoadFromASNViewSettings(*annots))
            ERR_POST(Error << "Error loading user annotations or camera settings");
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

    BioseqMap::iterator i, ie = bioseqs.end();
    for (i=bioseqs.begin(); i!=ie; i++) BioseqFree(i->second);
}

BioseqPtr StructureSet::GetOrCreateBioseq(const Sequence *sequence)
{
    if (!sequence || !sequence->isProtein) {
        ERR_POST(Error << "StructureSet::GetOrCreateBioseq() - got non-protein or NULL Sequence");
        return NULL;
    }

    // if already done
    BioseqMap::const_iterator b = bioseqs.find(sequence);
    if (b != bioseqs.end()) return b->second;

    // create new Bioseq and fill it in from Sequence data
    BioseqPtr bioseq = BioseqNew();

    bioseq->mol = Seq_mol_aa;
    bioseq->seq_data_type = Seq_code_ncbieaa;
    bioseq->repr = Seq_repr_raw;

    bioseq->length = sequence->Length();
    bioseq->seq_data = BSNew(bioseq->length);
    BSWrite(bioseq->seq_data, const_cast<char*>(sequence->sequenceString.c_str()), bioseq->length);

    // create Seq-id
    sequence->AddCSeqId(&(bioseq->id), true);

    // store Bioseq
    bioseqs[sequence] = bioseq;

    return bioseq;
}

void StructureSet::CreateAllBioseqs(const BlockMultipleAlignment *multiple)
{
    for (int row=0; row<multiple->NRows(); row++)
        GetOrCreateBioseq(multiple->GetSequenceOfRow(row));
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
    featSet.GetObject().SetId().Set(NO_DOMAIN);
    structureAlignments->SetFeatures().resize(1, featSet);

    // flag a change in data
    dataManager->SetDataChanged(eStructureAlignmentData);
}

void StructureSet::AddStructureAlignment(CBiostruc_feature *feature,
    int masterDomainID, int slaveDomainID)
{
    CBiostruc_annot_set *structureAlignments = dataManager->GetStructureAlignments();
    if (!structureAlignments) {
        ERR_POST(Warning << "StructureSet::AddStructureAlignment() - creating new structure alignment list");
        InitStructureAlignments(objects.front()->mmdbID);
        structureAlignments = dataManager->GetStructureAlignments();
    }

    // check master domain ID, to see if alignments have crossed master's domain boundaries
    int *currentMasterDomainID = &(structureAlignments->SetFeatures().front().GetObject().SetId().Set());
    if (*currentMasterDomainID == NO_DOMAIN)
        *currentMasterDomainID = masterDomainID;
    else if ((*currentMasterDomainID % 100) != (masterDomainID % 100))
        *currentMasterDomainID = (*currentMasterDomainID / 100) * 100;

    // check to see if this slave domain already has an alignment; if so, increment alignment #
    CBiostruc_feature_set::TFeatures::const_iterator
        f, fe = structureAlignments->GetFeatures().front().GetObject().GetFeatures().end();
    for (f=structureAlignments->GetFeatures().front().GetObject().GetFeatures().begin(); f!=fe; f++) {
        if ((f->GetObject().GetId().Get() / 10) == (slaveDomainID / 10))
            slaveDomainID++;
    }
    CRef<CBiostruc_feature_id> id(new CBiostruc_feature_id(slaveDomainID));
    feature->SetId(*id);

    CRef<CBiostruc_feature> featureRef(feature);
    structureAlignments->SetFeatures().front().GetObject().SetFeatures().resize(
        structureAlignments->GetFeatures().front().GetObject().GetFeatures().size() + 1, featureRef);

    // flag a change in data
    dataManager->SetDataChanged(eStructureAlignmentData);
}

void StructureSet::RemoveStructureAlignments(void)
{
    dataManager->SetStructureAlignments(NULL);
    // flag a change in data
    dataManager->SetDataChanged(eStructureAlignmentData);
}

void StructureSet::ReplaceAlignmentSet(const AlignmentSet *newAlignmentSet)
{
    ASNDataManager::SeqAnnotList *seqAnnots = dataManager->GetOrCreateSequenceAlignments();
    if (!seqAnnots) {
        ERR_POST(Error << "StructureSet::ReplaceAlignmentSet() - "
            << "can't figure out where in the asn the alignments are to go");
        return;
    }

    // update the AlignmentSet
    if (alignmentSet) {
        _RemoveChild(alignmentSet);
        delete alignmentSet;
    }
    alignmentSet = newAlignmentSet;

    // update the asn alignments
    seqAnnots->resize(alignmentSet->newAsnAlignmentData->size());
    ASNDataManager::SeqAnnotList::iterator o = seqAnnots->begin();
    ASNDataManager::SeqAnnotList::const_iterator n, ne = alignmentSet->newAsnAlignmentData->end();
    for (n=alignmentSet->newAsnAlignmentData->begin(); n!=ne; n++, o++)
        o->Reset(n->GetPointer());   // copy each Seq-annot CRef

    dataManager->SetDataChanged(eAlignmentData);
}

void StructureSet::ReplaceUpdates(const ncbi::objects::CCdd::TPending& newUpdates)
{
    dataManager->ReplaceUpdates(newUpdates);
}

void StructureSet::RemoveUnusedSequences(void)
{
	ASNDataManager::SequenceList updateSequences;
    if (alignmentManager) alignmentManager->GetUpdateSequences(&updateSequences);
    dataManager->RemoveUnusedSequences(alignmentSet, updateSequences);
}

bool StructureSet::SaveASNData(const char *filename, bool doBinary)
{
    // force a save of any edits to alignment and updates first (it's okay if this has already been done)
    GlobalMessenger()->SequenceWindowsSave();
    if (dataManager->HasDataChanged()) RemoveUnusedSequences();

    // create and temporarily attach a style dictionary, and annotation set + camera info
    // to the data (and then remove it again, so it's never out of date)
    CRef < CCn3d_style_dictionary > styleDictionary(styleManager->CreateASNStyleDictionary());
    dataManager->SetStyleDictionary(*styleDictionary);
    CRef < CCn3d_user_annotations > userAnnotations(new CCn3d_user_annotations());
    if (!styleManager->SaveToASNUserAnnotations(userAnnotations.GetPointer()) ||
        (objects.size() >= 1 && !renderer->SaveToASNViewSettings(userAnnotations.GetPointer()))) {
        ERR_POST(Error << "StructureSet::SaveASNData() - error creating user annotations blob");
        return false;
    }
    if (userAnnotations->IsSetAnnotations() || userAnnotations->IsSetView())
        dataManager->SetUserAnnotations(*userAnnotations);

    std::string err;
    bool writeOK = dataManager->WriteDataToFile(filename, doBinary, &err, eFNP_Replace);

    // remove style dictionary and annotations from asn
    dataManager->RemoveStyleDictionary();
    dataManager->RemoveUserAnnotations();

    if (writeOK) {
        dataManager->SetDataUnchanged();
    } else {
        ERR_POST(Error << "Write failed: " << err);
    }
    return writeOK;
}

// because the frame map (for each frame, a list of diplay lists) is complicated
// to create, this just verifies that all display lists occur exactly once
// in the map. Also, make sure that total # display lists in all frames adds up.
void StructureSet::VerifyFrameMap(void) const
{
    for (unsigned int l=OpenGLRenderer::FIRST_LIST; l<=lastDisplayList; l++) {
        bool found = false;
        for (unsigned int f=0; f<frameMap.size(); f++) {
            DisplayLists::const_iterator d, de=frameMap[f].end();
            for (d=frameMap[f].begin(); d!=de; d++) {
                if (*d == l) {
                    if (!found)
                        found = true;
                    else
                        ERR_POST(Fatal << "frameMap: repeated display list " << l);
                }
            }
        }
        if (!found)
            ERR_POST(Fatal << "display list " << l << " not in frameMap");
    }

    unsigned int nLists = 0;
    for (unsigned int f=0; f<frameMap.size(); f++) {
        DisplayLists::const_iterator d, de=frameMap[f].end();
        for (d=frameMap[f].begin(); d!=de; d++) nLists++;
    }
    if (nLists != lastDisplayList)
        ERR_POST(Fatal << "frameMap has too many display lists");
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
    for (int i=0; i<2; i++) {
        if (given && i==0) continue; // skip center calculation if given one
        ObjectList::const_iterator o, oe=objects.end();
        for (o=objects.begin(); o!=oe; o++) {
            StructureObject::CoordSetList::const_iterator c, ce=(*o)->coordSets.end();
            for (c=(*o)->coordSets.begin(); c!=ce; c++) {
                AtomSet::AtomMap::const_iterator a, ae=(*c)->atomSet->atomMap.end();
                for (a=(*c)->atomSet->atomMap.begin(); a!=ae; a++) {
                    Vector site(a->second.front()->site);
                    if ((*o)->IsSlave() && (*o)->transformToMaster)
                        ApplyTransformation(&site, *((*o)->transformToMaster));
                    if (i==0) {
                        siteSum += site;
                        nAtoms++;
                    } else {
                        dist = (site - center).length();
                        if (dist > maxDistFromCenter)
                            maxDistFromCenter = dist;
                    }
                }
            }
        }
        if (i==0) center = siteSum / nAtoms;
    }
    TESTMSG("center: " << center << ", maxDistFromCenter " << maxDistFromCenter);
    rotationCenter = center;
}

bool StructureSet::Draw(const AtomSet *atomSet) const
{
    TESTMSG("drawing StructureSet");
    if (!styleManager->CheckGlobalStyleSettings()) return false;
    return true;
}

unsigned int StructureSet::CreateName(const Residue *residue, int atomID)
{
    lastAtomName++;
    nameMap[lastAtomName] = std::make_pair(residue, atomID);
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
        TESTMSG("nothing selected");
        return;
    }

    // add highlight
    const Molecule *molecule;
    if (!residue->GetParentOfType(&molecule)) return;
    GlobalMessenger()->ToggleHighlight(molecule, residue->id, true);
    TESTMSG("selected " << molecule->identifier->ToString()
        << " residue " << residue->id << " (PDB: " << residue->nameGraph << ' ' << residue->namePDB
        << ") atom " << atomID << " (PDB: " << residue->GetAtomInfo(atomID)->name << ')');
    lastAtomSelected = name;

    // if indicated, use atom site as rotation center; use coordinate from first CoordSet, default altConf
    if (setCenter) {
        const StructureObject *object;
        if (!molecule->GetParentOfType(&object)) return;
        TESTMSG("rotating about " << object->pdbID
            << " molecule " << molecule->id << " residue " << residue->id << ", atom " << atomID);
        object->coordSets.front()->atomSet->SetActiveEnsemble(NULL);
        rotationCenter = object->coordSets.front()->atomSet->
            GetAtom(AtomPntr(molecule->id, residue->id, atomID)) ->site;
        if (object->IsSlave())
            ApplyTransformation(&rotationCenter, *(object->transformToMaster));
    }
}

void StructureSet::SelectByDistance(double cutoff, bool residuesOnly) const
{
    const Residue *residue;
    int atomID;
    if (lastAtomSelected == OpenGLRenderer::NO_NAME ||
        !GetAtomFromName(lastAtomSelected, &residue, &atomID)) {
        ERR_POST(Warning << "No previous atom selection!");
        return;
    }
    const StructureObject *object;
    if (!residue->GetParentOfType(&object)) return;
    object->SelectByDistance(residue, atomID, cutoff, residuesOnly);
}

const Sequence * StructureSet::CreateNewSequence(ncbi::objects::CBioseq& bioseq)
{
    // add Sequence to SequenceSet
    SequenceSet *modifiableSet = const_cast<SequenceSet*>(sequenceSet);
    const Sequence *newSeq = new Sequence(modifiableSet, bioseq);
    modifiableSet->sequences.push_back(newSeq);

    // add asn sequence to asn data
    if (dataManager->GetSequences()) {
        CSeq_entry *se = new CSeq_entry();
        se->SetSeq(bioseq);
        dataManager->GetSequences()->push_back(CRef<CSeq_entry>(se));
    } else
        ERR_POST(Error << "StructureSet::CreateNewSequence() - no sequence list in asn data");
    dataManager->SetDataChanged(eSequenceData);

    return newSeq;
}

void StructureSet::RejectAndPurgeSequence(const Sequence *reject, std::string reason, bool purge)
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

// trivial methods...
bool StructureSet::IsMultiStructure(void) const { return !dataManager->IsSingleStructure(); }
bool StructureSet::HasDataChanged(void) const { return dataManager->HasDataChanged(); }
void StructureSet::SetDataChanged(eDataChanged what) const { dataManager->SetDataChanged(what); }
bool StructureSet::IsCDD(void) const { return dataManager->IsCDD(); }
const std::string& StructureSet::GetCDDName(void) const { return dataManager->GetCDDName(); }
bool StructureSet::SetCDDName(const std::string& name) { return dataManager->SetCDDName(name); }
const std::string& StructureSet::GetCDDDescription(void) const { return dataManager->GetCDDDescription(); }
bool StructureSet::SetCDDDescription(const std::string& descr) { return dataManager->SetCDDDescription(descr); }
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
    for (j=biostruc.GetId().begin(); j!=je; j++) {
        if (j->GetObject().IsMmdb_id()) {
            mmdbID = j->GetObject().GetMmdb_id().Get();
            break;
        }
    }
    TESTMSG("MMDB id " << mmdbID);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        CBiostruc::TDescr::const_iterator k, ke=biostruc.GetDescr().end();
        for (k=biostruc.GetDescr().begin(); k!=ke; k++) {
            if (k->GetObject().IsName()) {
                pdbID = k->GetObject().GetName();
                break;
            }
        }
    }
    TESTMSG("PDB id " << pdbID);

    // get atom and feature spatial coordinates
    if (biostruc.IsSetModel()) {
        // iterate SEQUENCE OF Biostruc-model
        CBiostruc::TModel::const_iterator i, ie=biostruc.GetModel().end();
        for (i=biostruc.GetModel().begin(); i!=ie; i++) {

            // don't know how to deal with these...
            if (i->GetObject().GetType() == eModel_type_ncbi_vector ||
                i->GetObject().GetType() == eModel_type_other) continue;

//            // special case, typically for loading CDD's, when we're only interested in a single model type
//            if (isRawBiostrucFromMMDB && i->GetObject().GetType() != eModel_type_ncbi_all_atom) continue;

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

    TESTMSG("temperature range: " << minTemperature << " to " << maxTemperature);

    // get graph - must be done after atom coordinates are loaded, so we can
    // avoid storing graph nodes for atoms not present in the model
    graph = new ChemicalGraph(this, biostruc.GetChemical_graph(), biostruc.GetFeatures());
}

bool StructureObject::SetTransformToMaster(const CBiostruc_annot_set& annot, int masterMMDBID)
{
    CBiostruc_annot_set::TFeatures::const_iterator f1, f1e=annot.GetFeatures().end();
    for (f1=annot.GetFeatures().begin(); f1!=f1e; f1++) {
        CBiostruc_feature_set::TFeatures::const_iterator f2, f2e=f1->GetObject().GetFeatures().end();
        for (f2=f1->GetObject().GetFeatures().begin(); f2!=f2e; f2++) {

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
                    TESTMSG("got transform for " << pdbID << "->master");

                    // unpack transform into matrix, moves in reverse order;
                    Matrix xform;
                    transformToMaster = new Matrix();
                    CTransform::TMoves::const_iterator
                        m, me=graphAlign.GetTransform().front().GetObject().GetMoves().end();
                    for (m=graphAlign.GetTransform().front().GetObject().GetMoves().begin(); m!=me; m++) {
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

    ERR_POST(Warning << "Can't get structure alignment for slave " << pdbID
        << " with master " << masterMMDBID << ";\nwill likely require manual realignment");
    return false;
}

static void AddDomain(int *domain, const Molecule *molecule, const Block::Range *range)
{
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return;
    for (int l=range->from; l<=range->to; l++) {
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
    const Vector * const *masterCoords, const Vector * const *slaveCoords,
    const double *weights, int slaveRow)
{
    Vector masterCOM, slaveCOM; // centers of mass for master, slave
    Matrix slaveRotation;       // rotation to align slave with master
    if (!transformToMaster) transformToMaster = new Matrix();

    // do the fit
    RigidBodyFit(nCoords, masterCoords, slaveCoords, weights, masterCOM, slaveCOM, slaveRotation);

    // apply the resulting transform elements from the fit to this object's transform Matrix
    Matrix single, combined;
    SetTranslationMatrix(&single, -slaveCOM);
    ComposeInto(transformToMaster, slaveRotation, single);
    combined = *transformToMaster;
    SetTranslationMatrix(&single, masterCOM);
    ComposeInto(transformToMaster, single, combined);

    const BlockMultipleAlignment *multiple = parentSet->alignmentManager->GetCurrentMultipleAlignment();

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
        *slaveMID = new CMmdb_id(mmdbID);
    CBiostruc_id
        *masterBID = new CBiostruc_id(),
        *slaveBID = new CBiostruc_id();
    masterBID->SetMmdb_id(*masterMID);
    slaveBID->SetMmdb_id(*slaveMID);
    graphAlignment->SetBiostruc_ids().resize(2);
    graphAlignment->SetBiostruc_ids().front().Reset(masterBID);
    graphAlignment->SetBiostruc_ids().back().Reset(slaveBID);
    graphAlignment->SetAlignment().resize(2);

    // fill out sequence alignment intervals, tracking domains in alignment
    int masterDomain = NO_DOMAIN, slaveDomain = NO_DOMAIN;
    const Molecule
        *masterMolecule = multiple->GetSequenceOfRow(0)->molecule,
        *slaveMolecule = multiple->GetSequenceOfRow(slaveRow)->molecule;
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks(multiple->GetUngappedAlignedBlocks());
    if (blocks.get()) {
        CChem_graph_pntrs
            *masterCGPs = new CChem_graph_pntrs(),
            *slaveCGPs = new CChem_graph_pntrs();
        graphAlignment->SetAlignment().front().Reset(masterCGPs);
        graphAlignment->SetAlignment().back().Reset(slaveCGPs);
        CResidue_pntrs
            *masterRPs = new CResidue_pntrs(),
            *slaveRPs = new CResidue_pntrs();
        masterCGPs->SetResidues(*masterRPs);
        slaveCGPs->SetResidues(*slaveRPs);

        masterRPs->SetInterval().resize(blocks->size());
        slaveRPs->SetInterval().resize(blocks->size());
        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();
        CResidue_pntrs::TInterval::iterator
            mi = masterRPs->SetInterval().begin(),
            si = slaveRPs->SetInterval().begin();
        for (b=blocks->begin(); b!=be; b++, mi++, si++) {
            CResidue_interval_pntr
                *masterRIP = new CResidue_interval_pntr(),
                *slaveRIP = new CResidue_interval_pntr();
            mi->Reset(masterRIP);
            si->Reset(slaveRIP);

            masterRIP->SetMolecule_id().Set(masterMolecule->id);
            slaveRIP->SetMolecule_id().Set(slaveMolecule->id);

            const Block::Range *range = (*b)->GetRangeOfRow(0);
            masterRIP->SetFrom().Set(range->from + 1); // +1 to convert seqLoc to residueID
            masterRIP->SetTo().Set(range->to + 1);
            AddDomain(&masterDomain, masterMolecule, range);

            range = (*b)->GetRangeOfRow(slaveRow);
            slaveRIP->SetFrom().Set(range->from + 1);
            slaveRIP->SetTo().Set(range->to + 1);
            AddDomain(&slaveDomain, slaveMolecule, range);
        }
    }

    // fill out structure alignment transform
    CTransform *xform = new CTransform();
    graphAlignment->SetTransform().resize(1);
    graphAlignment->SetTransform().front().Reset(xform);
    xform->SetId(1);
    xform->SetMoves().resize(3);
    CTransform::TMoves::iterator m = xform->SetMoves().begin();
    for (int i=0; i<3; i++, m++) {
        CMove *move = new CMove();
        m->Reset(move);
        static const int scaleFactor = 100000;
        if (i == 0) {   // translate slave so its COM is at origin
            CTrans_matrix *trans = new CTrans_matrix();
            move->SetTranslate(*trans);
            trans->SetScale_factor(scaleFactor);
            trans->SetTran_1((int)(-(slaveCOM.x * scaleFactor)));
            trans->SetTran_2((int)(-(slaveCOM.y * scaleFactor)));
            trans->SetTran_3((int)(-(slaveCOM.z * scaleFactor)));
        } else if (i == 1) {
            CRot_matrix *rot = new CRot_matrix();
            move->SetRotate(*rot);
            rot->SetScale_factor(scaleFactor);
            rot->SetRot_11((int)(slaveRotation[0] * scaleFactor));
            rot->SetRot_12((int)(slaveRotation[4] * scaleFactor));
            rot->SetRot_13((int)(slaveRotation[8] * scaleFactor));
            rot->SetRot_21((int)(slaveRotation[1] * scaleFactor));
            rot->SetRot_22((int)(slaveRotation[5] * scaleFactor));
            rot->SetRot_23((int)(slaveRotation[9] * scaleFactor));
            rot->SetRot_31((int)(slaveRotation[2] * scaleFactor));
            rot->SetRot_32((int)(slaveRotation[6] * scaleFactor));
            rot->SetRot_33((int)(slaveRotation[10] * scaleFactor));
        } else if (i == 2) {    // translate slave so its COM is at COM of master
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
    if (slaveDomain == NO_DOMAIN) slaveDomain = 0;
    const StructureObject *masterObject;
    if (!masterMolecule->GetParentOfType(&masterObject)) return;
    int
        masterDomainID = masterObject->mmdbID*10000 + masterMolecule->id*100 + masterDomain,
        slaveDomainID = mmdbID*100000 + slaveMolecule->id*1000 + slaveDomain*10 + 1;
    parentSet->AddStructureAlignment(feature, masterDomainID, slaveDomainID);

    // for backward-compatibility with Cn3D 3.5, need name to encode chain/domain
    CNcbiOstrstream oss;
    oss << masterMolecule->identifier->pdbID << ((char) masterMolecule->identifier->pdbChain) << masterDomain << ' '
        << slaveMolecule->identifier->pdbID << ((char) slaveMolecule->identifier->pdbChain) << slaveDomain << ' '
        << "Structure alignment of slave " << multiple->GetSequenceOfRow(slaveRow)->identifier->ToString()
        << " with master " << multiple->GetSequenceOfRow(0)->identifier->ToString()
        << ", as computed by Cn3D" << '\0';
    feature->SetName(std::string(oss.str()));
    delete oss.str();
}

void StructureObject::SelectByDistance(
    const Residue *residue, int atomID, double cutoff, bool residuesOnly) const
{
    // remove all highlights
    GlobalMessenger()->RemoveAllHighlights(true);

    // get coord of given atom
    const Molecule *molecule;
    if (!residue->GetParentOfType(&molecule)) return;
    const AtomCoord *selected = coordSets.front()->atomSet->
        GetAtom(AtomPntr(molecule->id, residue->id, atomID), true, false);
    if (!selected) {
        ERR_POST(Error << "Can't get coordinates for selected atom!");
        return;
    }

    // loop over all molecules
    ChemicalGraph::MoleculeMap::const_iterator m, me = graph->molecules.end();
    for (m=graph->molecules.begin(); m!=me; m++) {

        // do only biopolymers if residuesOnly is true
        if (residuesOnly && !(m->second->IsProtein() || m->second->IsNucleotide())) continue;

        // loop over all residues
        Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
        for (r=m->second->residues.begin(); r!=re; r++) {

            // always highlight selected residue
            if (r->second == residue) {
                GlobalMessenger()->ToggleHighlight(m->second, r->second->id);
                continue;
            }

            // loop through atoms (using default coordinate for each)
            const Residue::AtomInfoMap& atomInfos = r->second->GetAtomInfos();
            Residue::AtomInfoMap::const_iterator a, ae = atomInfos.end();
            for (a=atomInfos.begin(); a!=ae; a++) {
                const AtomCoord *atomCoord = coordSets.front()->atomSet->
                    GetAtom(AtomPntr(m->second->id, r->second->id, a->first), true, true);

                // if any atom is < cutoff away, highlight that residue
                if (atomCoord && (atomCoord->site - selected->site).length() <= cutoff) {
                    GlobalMessenger()->ToggleHighlight(m->second, r->second->id);
                    break;
                }
            }
        }
    }
}

END_SCOPE(Cn3D)
