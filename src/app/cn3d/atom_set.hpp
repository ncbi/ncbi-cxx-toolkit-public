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
*      Classes to hold sets of atom data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2001/05/31 18:46:25  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.12  2000/10/04 17:40:44  thiessen
* rearrange STL #includes
*
* Revision 1.11  2000/08/25 14:21:32  thiessen
* minor tweaks
*
* Revision 1.10  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.9  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.8  2000/07/17 22:36:45  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.7  2000/07/16 23:18:33  thiessen
* redo of drawing system
*
* Revision 1.6  2000/07/12 23:28:27  thiessen
* now draws basic CPK model
*
* Revision 1.5  2000/07/12 02:00:39  thiessen
* add basic wxWindows GUI
*
* Revision 1.4  2000/07/11 13:49:25  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.3  2000/07/01 15:44:23  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.2  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.1  2000/06/29 14:35:20  thiessen
* new atom_set files
*
* ===========================================================================
*/

#ifndef CN3D_ATOMSET__HPP
#define CN3D_ATOMSET__HPP

#include <corelib/ncbistl.hpp>

#include <string>
#include <map>

#include <objects/mmdb2/Atomic_coordinates.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/vector_math.hpp"

BEGIN_SCOPE(Cn3D)

class Vector;

// An AtomSet is a list of Atom records, accessible through the equivalent of
// an ASN1 Atom-pntr (molecule, residue, and atom IDs) plus an optional alternate
// conformer ID. An Atom contains the spatial coordinates and any temperature,
// occupancy, and alternate conformer data present for an atom.

class AtomCoord : public StructureBase
{
public:
    AtomCoord(StructureBase *parent);

    // public data
    Vector site;
    double averageTemperature; // average of 6 factors if anisotropic
    double occupancy;
    char altConfID;
    static const double NO_TEMPERATURE;
    static const double NO_OCCUPANCY;
    static const double NO_ALTCONFID;

    // public methods
    bool HasTemp(void) const { return (averageTemperature!=NO_TEMPERATURE); }
    bool HasOccup(void) const { return (occupancy!=NO_OCCUPANCY); }
    bool HasAlt(void) const { return (altConfID!=NO_ALTCONFID); }

private:
};

class StructureSet;

class AtomSet : public StructureBase
{
    friend StructureSet;
public:
    AtomSet(StructureBase *parent, const ncbi::objects::CAtomic_coordinates& coords);
    ~AtomSet(void);

    // public data
    typedef std::list < const std::string * > EnsembleList;
    EnsembleList ensembles;

    // public methods

    // which ensemble to use?
    bool SetActiveEnsemble(const std::string *ensemble);
    // get Atom based on Atom-pntr. If 'getAny' is true, then will return arbitrary
    // altConf; if false, will only return one from active ensemble
    const AtomCoord* GetAtom(const AtomPntr& atom,
        bool getAny = false, bool suppressWarning = false) const;

private:
    // this provides a convenient way to look up atoms from Atom-pntr info
    typedef std::pair < int, std::pair < int, int > > AtomPntrKey;
    AtomPntrKey MakeKey(const AtomPntr& ap) const
    {
        return std::make_pair(ap.mID, std::make_pair(ap.rID, ap.aID));
    }
    typedef std::list < const AtomCoord * > AtomAltList;
    typedef std::map < AtomPntrKey, AtomAltList > AtomMap;
    AtomMap atomMap;
    const std::string *activeEnsemble;

public:
    const std::string* GetActiveEnsemble(void) const { return activeEnsemble; }
    bool HasTemp(void) const { return (atomMap.size()>0 && (*(atomMap.begin()->second.begin()))->HasTemp()); }
    bool HasOccup(void) const { return (atomMap.size()>0 && (*(atomMap.begin()->second.begin()))->HasOccup()); }
    bool HasAlt(void) const { return (atomMap.size()>0 && (*(atomMap.begin()->second.begin()))->HasAlt()); }
};

END_SCOPE(Cn3D)

#endif // CN3D_ATOMSET__HPP
