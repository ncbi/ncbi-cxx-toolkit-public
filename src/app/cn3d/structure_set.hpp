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
    StructureSet(ncbi::objects::CNcbi_mime_asn1 *mime, unsigned int structureLimit, OpenGLRenderer *r);
    StructureSet(ncbi::objects::CCdd *cdd, unsigned int structureLimit, OpenGLRenderer *r);
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

    // to map display list -> dependent transform
    typedef std::map < unsigned int, const Matrix * const * > TransformMap;
    TransformMap transformMap;

    // for ensuring unique structure<->structure alignments for repeated structures
    std::map < int, bool > usedFeatures;


    // public methods

    bool IsMultiStructure(void) const;

    // set screen and rotation center of model (coordinate relative to Master);
    // if NULL, will calculate average geometric center
    void SetCenter(const Vector *setTo = NULL);

    // try to find an "optimal" view of a single structure
    void CenterViewOnStructure(void);

    // center rotation and view on aligned residues only
    bool CenterViewOnAlignedResidues(void);

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

    // adds a new Sequence to the SequenceSet if it doesn't exist already
    const Sequence * FindOrCreateSequence(ncbi::objects::CBioseq& bioseq);

    // reject sequence (from CDD)
    void RejectAndPurgeSequence(const Sequence *reject, std::string reason, bool purge);
    typedef std::list < ncbi::CRef < ncbi::objects::CReject_id > > RejectList;
    const RejectList * GetRejects(void) const;
    void ShowRejects(void) const;

    // adds a new Biostruc to the asn data, if appropriate
    bool AddBiostrucToASN(ncbi::objects::CBiostruc *biostruc);

    // for manipulating structure alignment features
    void InitStructureAlignments(int masterMMDBID);
    void AddStructureAlignment(ncbi::objects::CBiostruc_feature *feature,
        int masterDomainID, int dependentDomainID);
    void RemoveStructureAlignments(void);

    bool MonitorAlignments(void) const;

private:
    ASNDataManager *dataManager;

    // data preparation methods
    void Load(unsigned int structureLimit);
    void LoadSequencesForSingleStructure(void);             // for single structures
    void LoadAlignmentsAndStructures(unsigned int structureLimit);   // for alignments
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
        const Vector * const *masterCoords, const Vector * const *dependentCoords,
        const double *weights, int dependentRow);

    bool IsMaster(void) const { return isMaster; }
    bool IsDependent(void) const { return !isMaster; }
    int NDomains(void) const { return domainMap.size(); }
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP
