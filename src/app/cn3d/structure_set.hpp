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
* ===========================================================================
*/

#ifndef CN3D_STRUCTURESET__HPP
#define CN3D_STRUCTURESET__HPP

#include <corelib/ncbistl.hpp>

#include <string>
#include <map>
#include <vector>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/cdd/Align_annot_set.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/vector_math.hpp"


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

class StructureSet : public StructureBase
{
public:
    StructureSet(ncbi::objects::CNcbi_mime_asn1 *mime);
    // to load in a CDD, need "dataDir" to be pathname to dir. that contains MMDB Biostrucs,
    // must end with trailing path separator; load at most structureLimit # Biostrucs
    StructureSet(ncbi::objects::CCdd *cdd, const char *dataDir, int structureLimit);
    ~StructureSet(void);

    // public data
    const bool isMultipleStructure;
    bool isAlphaOnly;   // assume if one Object is alpha-only, then they all are
    int nDomains;       // total number of domains over all objects

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

    // put in new AlignmentSet - e.g. when alignment has been edited
    void ReplaceAlignmentSet(const AlignmentSet *newAlignmentSet);

    // replace the ASN update list with the current updates
    void ReplaceUpdates(const ncbi::objects::CCdd::TPending& newUpdates);

    // set screen and rotation center of model (coordinate relative to Master);
    // if NULL, will calculate average geometric center
    void SetCenter(const Vector *setTo = NULL);

    bool Draw(const AtomSet *atomSet) const;

    // keep a list of names to look atoms from GL selection
    unsigned int CreateName(const Residue *residue, int atomID);
    bool GetAtomFromName(unsigned int name, const Residue **residue, int *atomID);

    // called when an atom is selected in the GL window. If setCenter == true, then
    // the atom's location is used as the global rotation center
    void SelectedAtom(unsigned int name, bool setCenter);

    // writes data to a file; returns true on success
    bool SaveASNData(const char *filename, bool doBinary);

private:
    // pointers to the asn data
    ncbi::objects::CNcbi_mime_asn1 *mimeData;
    ncbi::objects::CCdd *cddData;
    ncbi::objects::CBiostruc_annot_set *structureAlignments;

    void Init(void);
    void MatchSequencesToMolecules(void);
    void VerifyFrameMap(void) const;
    typedef std::pair < const Residue*, int > NamePair;
    typedef std::map < unsigned int, NamePair > NameMap;
    NameMap nameMap;
    unsigned int lastAtomName;

    // updates sequences in the asn (but not in the SequenceSet), to remove any sequences
    // that are not used by the current alignmentSet or updates
    void RemoveUnusedSequences(void);

    // flags to tell whether various parts of the data have been changed
    enum eDataChanged {
        eAlignmentData              = 0x01,
        eStructureAlignmentData     = 0x02,
        eSequenceData               = 0x04,
        eUpdateData                 = 0x08,
        eStyleData                  = 0x100,
        eUserAnnotationData         = 0x200,
        eOtherData                  = 0x400
    };
    mutable unsigned int dataChanged;

public:
    bool HasDataChanged(void) const { return (dataChanged > 0); }
    void StyleDataChanged(void) const { dataChanged |= eStyleData; }
    void UserAnnotationDataChanged(void) const { dataChanged |= eUserAnnotationData; }

    // for manipulating structure alignment features
    void InitStructureAlignments(int masterMMDBID);
    void AddStructureAlignment(ncbi::objects::CBiostruc_feature *feature,
        int masterDomainID, int slaveDomainID);
    void RemoveStructureAlignments(void);

    // CDD-specific stuff
    bool IsCDD(void) const { return (cddData != NULL); }
    const std::string& GetCDDDescription(void) const;
    bool SetCDDDescription(const std::string& descr);
    typedef std::vector < std::string > TextLines;
    bool GetCDDNotes(TextLines *lines) const;
    bool SetCDDNotes(const TextLines& lines);
    ncbi::objects::CAlign_annot_set * GetCopyOfCDDAnnotSet(void) const;
    bool SetCDDAnnotSet(ncbi::objects::CAlign_annot_set *newAnnotSet);
};

class ChemicalGraph;
class CoordSet;

class StructureObject : public StructureBase
{
public:
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

    // set transform based on asn1 data
    bool SetTransformToMaster(const ncbi::objects::CBiostruc_annot_set& annot, int masterMMDBID);

    // set transform based on rigid body fit of given coordinates
    void RealignStructure(int nCoords,
        const Vector * const *masterCoords, const Vector * const *slaveCoords,
        const double *weights, int slaveRow);

private:
    const bool isMaster;

public:
    // isRawBiostrucFromMMDB says whether this is "raw" MMDB data - with all its various models
    StructureObject(StructureBase *parent,
        const ncbi::objects::CBiostruc& biostruc, bool isMaster, bool isRawBiostrucFromMMDB);
    ~StructureObject(void) { if (transformToMaster) delete transformToMaster; }

    bool IsMaster(void) const { return isMaster; }
    bool IsSlave(void) const { return !isMaster; }
    int NDomains(void) const { return static_cast<int>(domainMap.size()); }
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP
