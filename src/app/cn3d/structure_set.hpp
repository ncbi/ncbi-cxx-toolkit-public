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
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/cdd/Cdd.hpp>

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

class StructureSet : public StructureBase
{
public:
    StructureSet(const ncbi::objects::CNcbi_mime_asn1& mime);
    StructureSet(const ncbi::objects::CCdd& cdd);
    ~StructureSet(void);

    // public data
    const bool isMultipleStructure;
    bool isAlphaOnly;   // assume if one Object is alpha-only, then they all are
    int nDomains;       // total number of domains over all objects

    typedef LIST_TYPE < const StructureObject * > ObjectList;
    ObjectList objects;

    // sequence and alignment information
    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;
    AlignmentManager *alignmentManager;

    OpenGLRenderer *renderer;
    ShowHideManager *showHideManager;
    StyleManager *styleManager;

    Colors *colors;

    Vector center; // center of structure (relative to Master's coordinates)
    double maxDistFromCenter; // max distance of any atom from center
    Vector rotationCenter; // center of rotation (relative to Master's coordinates)

    // public methods

    void ReplaceAlignmentSet(const AlignmentSet *newAlignmentSet);

    // set screen and rotation center of model (coordinate relative to Master);
    // if NULL, will calculate average geometric center
    void SetCenter(const Vector *setTo = NULL);

    bool Draw(const AtomSet *atomSet) const;

    // keep a list of names to look atoms from GL selection
    unsigned int CreateName(const Residue *residue, int atomID);
    bool GetAtomFromName(unsigned int name, const Residue **residue, int *atomID);

    // called when an atom is selected in the GL window
    void SelectedAtom(unsigned int name);

    // for assigning display lists and frames
    unsigned int lastDisplayList;

    typedef LIST_TYPE < unsigned int > DisplayLists;
    typedef std::vector < DisplayLists > FrameMap;
    FrameMap frameMap;

    // to map display list -> slave transform
    typedef std::map < unsigned int, const Matrix * const * > TransformMap;
    TransformMap transformMap;

    // for ensuring unique structure<->structure alignments for repeated structures
    std::map < int, bool > usedFeatures;

    // do any necessary adjustments to original asn data before output; returns true
    // if successful, false if something went wrong - in which case data probably
    // should not be used
    bool PrepareMimeForOutput(ncbi::objects::CNcbi_mime_asn1& mime) const;

private:
    void Init(void);
    void MatchSequencesToMolecules(void);
    void VerifyFrameMap(void) const;
    typedef std::pair < const Residue*, int > NamePair;
    typedef std::map < unsigned int, NamePair > NameMap;
    NameMap nameMap;
    unsigned int lastAtomName;

    // flags to tell whether various parts of the data have been changed
    bool newAlignments;
};

class ChemicalGraph;
class CoordSet;

class StructureObject : public StructureBase
{
public:
    StructureObject(StructureBase *parent, const ncbi::objects::CBiostruc& biostruc, bool master);

    // public data

    static const int NO_MMDB_ID;
    int id, mmdbID;
    std::string pdbID;
    Matrix *transformToMaster;

    ~StructureObject(void) { if (transformToMaster) delete transformToMaster; }

    // an object has one ChemicalGraph that can be applied to one or more
    // CoordSets to generate the object's model(s)
    const ChemicalGraph *graph;
    typedef LIST_TYPE < const CoordSet * > CoordSetList;
    CoordSetList coordSets;

    // map of domainIDs -> Molecule
    typedef std::map < int, const Molecule * > DomainMap;
    DomainMap domainMap;

    // public methods

    // set transform based on asn1 data
    bool SetTransformToMaster(const ncbi::objects::CBiostruc_annot_set& annot, int masterMMDBID);

    // set transform based on rigid body fit of given coordinates
    void RealignStructure(int nCoords,
        const Vector * const *masterCoords, const Vector * const *slaveCoords, const double *weights);

private:
    const bool isMaster;

public:
    bool IsMaster(void) const { return isMaster; }
    bool IsSlave(void) const { return !isMaster; }
    int NDomains(void) const { return domainMap.size(); }
};


// a utility function for reading different types of ASN data from a file
template < class ASNClass >
static bool ReadASNFromFile(const char *filename, ASNClass& ASNobject, bool isBinary, std::string& err)
{
    // initialize the binary input stream
    auto_ptr<ncbi::CNcbiIstream> inStream;
    inStream.reset(new ncbi::CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
//        ERR_POST(ncbi::Error << "Cannot open file '" << filename << "' for reading");
        err = "Cannot open file for reading";
        return false;
    }

    auto_ptr<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(*inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(*inStream));
    }

    // Read the data
    try {
        err.erase();
        *inObject >> ASNobject;
    } catch (exception& e) {
//        ERR_POST(ncbi::Error << "Cannot read file '" << filename << "'\nreason: " << e.what());
        err = e.what();
        return false;
    }

    return true;
}

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP
