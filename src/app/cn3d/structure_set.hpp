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

#ifndef CN3D_STRUCTURESET__HPP
#define CN3D_STRUCTURESET__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/cdd/Align_annot_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/cdd/Reject_id.hpp>

#include <string>
#include <map>
#include <vector>
#include <list>

#include <objseq.h>
#include <objloc.h>

#include "structure_base.hpp"
#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// StructureSet is the top-level container. It holds a set of SturctureObjects;
// A SturctureObject is basically the contents of one PDB entry.

class StructureObject;
class OpenGLRenderer;
class ShowHideManager;
class StyleManager;
class Residue;
class SequenceSet;
class AlignmentSet;
class AlignmentManager;
class SequenceViewer;
class Messenger;
class Colors;
class Molecule;
class BlockMultipleAlignment;
class Sequence;
class SequenceSet;
class ChemicalGraph;
class CoordSet;
class ASNDataManager;

class StructureSet : public StructureBase
{
public:
    StructureSet(ncbi::objects::CNcbi_mime_asn1 *mime, int structureLimit, OpenGLRenderer *r);
    StructureSet(ncbi::objects::CCdd *cdd, int structureLimit, OpenGLRenderer *r);
    ~StructureSet(void);

    // public data

    bool isAlphaOnly;   // assume if one Object is alpha-only, then they all are
    int nDomains;       // total number of domains over all objects
    bool hasUserStyle;  // whether there's a global style in the original data

    typedef std::list < const StructureObject * > ObjectList;
    ObjectList objects;

    // sequence and alignment information
    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;
    AlignmentManager *alignmentManager;

    OpenGLRenderer *renderer;
    ShowHideManager *showHideManager;
    StyleManager *styleManager;

    Vector center; // center of structure (relative to Master's coordinates)
    double maxDistFromCenter; // max distance of any atom from center
    Vector rotationCenter; // center of rotation (relative to Master's coordinates)

    // for assigning display lists and frames
    unsigned int lastDisplayList;

    typedef std::list < unsigned int > DisplayLists;
    typedef std::vector < DisplayLists > FrameMap;
    FrameMap frameMap;

    // to map display list -> slave transform
    typedef std::map < unsigned int, const Matrix * const * > TransformMap;
    TransformMap transformMap;

    // for ensuring unique structure<->structure alignments for repeated structures
    std::map < int, bool > usedFeatures;


    // public methods

    bool IsMultiStructure(void) const;

    // set screen and rotation center of model (coordinate relative to Master);
    // if NULL, will calculate average geometric center
    void SetCenter(const Vector *setTo = NULL);

    // center rotation and view on aligned residues only
    void CenterViewOnAlignedResidues(void);

    bool Draw(const AtomSet *atomSet) const;

    // keep a list of names to look atoms from GL selection
    unsigned int CreateName(const Residue *residue, int atomID);
    bool GetAtomFromName(unsigned int name, const Residue **residue, int *atomID) const;

    // called when an atom is selected in the GL window. If setCenter == true, then
    // the atom's location is used as the global rotation center
    void SelectedAtom(unsigned int name, bool setCenter);

    // select various components based on distance
    static const unsigned int
        eSelectProtein,
        eSelectNucleotide,
        eSelectHeterogen,
        eSelectSolvent,
        eSelectOtherMoleculesOnly;
    void SelectByDistance(double cutoff, unsigned int options) const;

    // updates sequences in the asn, to remove any sequences
    // that are not used by the current alignmentSet or updates
    void RemoveUnusedSequences(void);

    // put in new AlignmentSet - e.g. when alignment has been edited
    void ReplaceAlignmentSet(AlignmentSet *newAlignmentSet);

    // replace the ASN update list with the current updates
    void ReplaceUpdates(ncbi::objects::CCdd::TPending& newUpdates);

    // bit flags to tell whether various parts of the data have been changed
    static const unsigned int
        ePSSMData,                  // PSSM values have changed
        eRowOrderData,              // row order has changed
        eAnyAlignmentData,          // any change to alignment (including edits that don't change prev two)
        eStructureAlignmentData,
        eSequenceData,
        eUpdateData,
        eStyleData,
        eUserAnnotationData,
        eCDDData,
        eOtherData;

    bool HasDataChanged(void) const;
    void SetDataChanged(unsigned int what) const;

    // CDD-specific data accessors
    bool IsCDD(void) const;
    bool IsCDDInMime(void) const;
    const std::string& GetCDDName(void) const;
    bool SetCDDName(const std::string& name);
    const std::string& GetCDDDescription(void) const;
    bool SetCDDDescription(const std::string& descr);
    typedef std::vector < std::string > TextLines;
    bool GetCDDNotes(TextLines *lines) const;
    bool SetCDDNotes(const TextLines& lines);
    ncbi::objects::CCdd_descr_set * GetCDDDescrSet(void);
    ncbi::objects::CAlign_annot_set * GetCDDAnnotSet(void);

    // convert underlying data from mime to cdd
    bool ConvertMimeDataToCDD(const std::string& cddName);

    // writes data to a file; returns true on success
    bool SaveASNData(const char *filename, bool doBinary, unsigned int *changeFlags);

    // adds a new Sequence to the SequenceSet
    const Sequence * CreateNewSequence(ncbi::objects::CBioseq& bioseq);

    // reject sequence (from CDD)
    void RejectAndPurgeSequence(const Sequence *reject, std::string reason, bool purge);
    typedef std::list < ncbi::CRef < ncbi::objects::CReject_id > > RejectList;
    const RejectList * GetRejects(void) const;
    void ShowRejects(void) const;

    // creates Bioseq from Sequence; registed with SeqMgr and stored in BioseqMap
    Bioseq * GetOrCreateBioseq(const Sequence *sequence);
    void CreateAllBioseqs(const BlockMultipleAlignment *multiple);

    // adds a new Biostruc to the asn data, if appropriate
    bool AddBiostrucToASN(ncbi::objects::CBiostruc *biostruc);

    // for manipulating structure alignment features
    void InitStructureAlignments(int masterMMDBID);
    void AddStructureAlignment(ncbi::objects::CBiostruc_feature *feature,
        int masterDomainID, int slaveDomainID);
    void RemoveStructureAlignments(void);

    bool MonitorAlignments(void) const;

private:
    ASNDataManager *dataManager;

    // data preparation methods
    void Load(int structureLimit);
    void LoadSequencesForSingleStructure(void);             // for single structures
    void LoadAlignmentsAndStructures(int structureLimit);   // for alignments
    std::map < const ncbi::objects::CBiostruc * , bool > usedStructures;
    bool MatchSequenceToMoleculeInObject(const Sequence *seq,
        const StructureObject *obj, const Sequence **seqHandle = NULL);
    bool LoadMaster(int masterMMDBID);
    void VerifyFrameMap(void) const;

    // to keep track of gl "name" -> atom correspondence (for structure picking)
    typedef std::pair < const Residue*, int > NamePair;
    typedef std::map < unsigned int, NamePair > NameMap;
    NameMap nameMap;
    unsigned int lastAtomName;

    // holds C Bioseqs associated with Sequences
    typedef std::map < const Sequence *, Bioseq * > BioseqMap;
    BioseqMap bioseqs;

    // for printing out distances between successively picked atoms
    Vector prevPickedAtomCoord;
    bool havePrevPickedAtomCoord;
};

class StructureObject : public StructureBase
{
private:
    const bool isMaster;

public:
    // biostruc must not be "raw" mmdb data - will get confused by presence of different models
    StructureObject(StructureBase *parent, const ncbi::objects::CBiostruc& biostruc, bool isMaster);
    ~StructureObject(void) { if (transformToMaster) delete transformToMaster; }

    // public data

    static const int NO_MMDB_ID;
    int id, mmdbID;
    std::string pdbID;
    Matrix *transformToMaster;

    // an object has one ChemicalGraph that can be applied to one or more
    // CoordSets to generate the object's model(s)
    const ChemicalGraph *graph;
    typedef std::list < const CoordSet * > CoordSetList;
    CoordSetList coordSets;

    // min and max atomic temperatures
    static const double NO_TEMPERATURE;
    double minTemperature, maxTemperature;

    // map of internal domainID -> Molecule and MMDB-assigned id
    typedef std::map < int, const Molecule * > DomainMap;
    DomainMap domainMap;
    typedef std::map < int, int > DomainIDMap;
    DomainIDMap domainID2MMDB;

    // public methods

    typedef std::map < const Residue *, const Molecule * > ResidueMap;
    void SelectByDistance(double cutoff, unsigned int options, ResidueMap *selectedResidues) const;

    // set transform based on asn1 data
    bool SetTransformToMaster(const ncbi::objects::CBiostruc_annot_set& annot, int masterMMDBID);

    // set transform based on rigid body fit of given coordinates
    void RealignStructure(int nCoords,
        const Vector * const *masterCoords, const Vector * const *slaveCoords,
        const double *weights, int slaveRow);

    bool IsMaster(void) const { return isMaster; }
    bool IsSlave(void) const { return !isMaster; }
    int NDomains(void) const { return domainMap.size(); }
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.81  2005/04/21 14:31:19  thiessen
* add MonitorAlignments()
*
* Revision 1.80  2004/10/05 14:57:54  thiessen
* add distance selection dialog
*
* Revision 1.79  2004/05/21 17:29:51  thiessen
* allow conversion of mime to cdd data
*
* Revision 1.78  2004/02/19 17:05:15  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.77  2003/07/17 20:13:51  thiessen
* don't ask for filename on save/termination with -f; add eAnyAlignmentData flag
*
* Revision 1.76  2003/07/17 16:52:34  thiessen
* add FileSaved message with edit typing
*
* Revision 1.75  2003/07/14 18:37:08  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.74  2003/02/03 19:20:07  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.73  2003/01/27 15:52:22  thiessen
* merge after highlighted row; show rejects; trim rejects from new reject list
*
* Revision 1.72  2002/11/21 15:26:33  thiessen
* fix style dictionary loading bug
*
* Revision 1.71  2002/11/19 21:19:44  thiessen
* more const changes for objects; fix user vs default style bug
*
* Revision 1.70  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.69  2002/09/26 17:32:13  thiessen
* show distance between picked atoms; show RMS for structure alignments
*
* Revision 1.68  2002/09/19 12:51:08  thiessen
* fix block aligner / update bug; add distance select for other molecules only
*
* Revision 1.67  2002/09/14 17:03:07  thiessen
* center initial view on aligned residues
*
* Revision 1.66  2002/06/05 17:50:09  thiessen
* title tweaks
*
* Revision 1.65  2002/04/10 13:16:29  thiessen
* new selection by distance algorithm
*
* Revision 1.64  2002/02/19 14:59:40  thiessen
* add CDD reject and purge sequence
*
* Revision 1.63  2002/02/12 17:19:22  thiessen
* first working structure import
*
* Revision 1.62  2002/01/03 16:18:40  thiessen
* add distance selection
*
* Revision 1.61  2001/12/06 23:13:46  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.60  2001/11/27 16:26:10  thiessen
* major update to data management system
*
* Revision 1.59  2001/10/17 17:46:27  thiessen
* save camera setup and rotation center in files
*
* Revision 1.58  2001/10/09 18:57:27  thiessen
* add CDD references editing dialog
*
* Revision 1.57  2001/10/08 00:00:02  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.56  2001/10/01 16:03:58  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.55  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.54  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.53  2001/08/15 20:53:58  juran
* Heed warnings.
*
* Revision 1.52  2001/08/13 22:30:52  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.51  2001/07/12 17:34:23  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.50  2001/07/10 16:39:34  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.49  2001/07/04 19:38:55  thiessen
* finish user annotation system
*
* Revision 1.48  2001/06/14 17:44:46  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.47  2001/06/14 00:33:23  thiessen
* asn additions
*
* Revision 1.46  2001/06/05 13:21:17  thiessen
* fix structure alignment list problems
*
* Revision 1.45  2001/05/31 18:46:27  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.44  2001/05/02 13:46:15  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.43  2001/04/17 20:15:24  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.42  2001/03/23 15:13:46  thiessen
* load sidechains in CDD's
*
* Revision 1.41  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.40  2001/02/16 00:36:48  thiessen
* remove unused sequences from asn data
*
* Revision 1.39  2001/02/13 01:03:04  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.38  2001/02/02 20:17:42  thiessen
* can read in CDD with multi-structure but no struct. alignments
*
* Revision 1.37  2001/01/18 19:37:01  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.36  2000/12/29 19:23:50  thiessen
* save row order
*
* Revision 1.35  2000/12/22 19:25:47  thiessen
* write cdd output files
*
* Revision 1.34  2000/12/21 23:42:25  thiessen
* load structures from cdd's
*
* Revision 1.33  2000/12/20 23:47:53  thiessen
* load CDD's
*
* Revision 1.32  2000/12/01 19:34:43  thiessen
* better domain assignment
*
* Revision 1.31  2000/11/30 15:49:09  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.30  2000/11/13 18:05:58  thiessen
* working structure re-superpositioning
*
* Revision 1.29  2000/11/12 04:02:22  thiessen
* working file save including alignment edits
*
* Revision 1.28  2000/11/11 21:12:08  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.27  2000/11/02 16:48:24  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.26  2000/10/04 17:40:47  thiessen
* rearrange STL #includes
*
* Revision 1.25  2000/09/15 19:24:33  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.24  2000/09/11 22:57:56  thiessen
* working highlighting
*
* Revision 1.23  2000/09/11 01:45:54  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.22  2000/08/30 19:49:05  thiessen
* working sequence window
*
* Revision 1.21  2000/08/29 04:34:15  thiessen
* working alignment manager, IBM
*
* Revision 1.20  2000/08/28 18:52:18  thiessen
* start unpacking alignments
*
* Revision 1.19  2000/08/27 18:50:56  thiessen
* extract sequence information
*
* Revision 1.18  2000/08/21 19:31:17  thiessen
* add style consistency checking
*
* Revision 1.17  2000/08/07 14:12:48  thiessen
* added animation frames
*
* Revision 1.16  2000/08/07 00:20:18  thiessen
* add display list mechanism
*
* Revision 1.15  2000/08/04 22:49:11  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.14  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.13  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.12  2000/07/18 16:49:44  thiessen
* more friendly rotation center setting
*
* Revision 1.11  2000/07/18 00:05:45  thiessen
* allow arbitrary rotation center
*
* Revision 1.10  2000/07/17 22:36:46  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.9  2000/07/17 04:21:10  thiessen
* now does correct structure alignment transformation
*
* Revision 1.8  2000/07/16 23:18:35  thiessen
* redo of drawing system
*
* Revision 1.7  2000/07/12 02:00:39  thiessen
* add basic wxWindows GUI
*
* Revision 1.6  2000/07/11 13:49:30  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.5  2000/07/01 15:44:23  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.4  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.3  2000/06/29 14:35:20  thiessen
* new atom_set files
*
* Revision 1.2  2000/06/28 13:08:13  thiessen
* store alt conf ensembles
*
* Revision 1.1  2000/06/27 20:08:14  thiessen
* initial checkin
*
*/
