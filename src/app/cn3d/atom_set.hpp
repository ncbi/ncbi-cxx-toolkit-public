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

#include <string>
#include <map>

#include <serial/serial.hpp>            
#include <objects/mmdb2/Atomic_coordinates.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/vector_math.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

#define ATOM_NO_TEMPERATURE (-1.0)
#define ATOM_NO_OCCUPANCY (-1.0)
#define ATOM_NO_ALTCONFID ('-')

class Atom : public StructureBase
{
public:
    Atom(StructureBase *parent);
    ~Atom(void);

    // public data
    Vector site;
    double averageTemperature; // average of 6 factors if anisotropic
    double occupancy;
    char altConfID;

    // public methods
    bool HasTemp(void) const { return (averageTemperature!=ATOM_NO_TEMPERATURE); }
    bool HasOccup(void) const { return (occupancy!=ATOM_NO_OCCUPANCY); }
    bool HasAlt(void) const { return (altConfID!=ATOM_NO_ALTCONFID); }
    void Draw(void) const;

private:
};

class AtomSet : public StructureBase
{
public:
    AtomSet(StructureBase *parent, const CAtomic_coordinates& coords);
    ~AtomSet(void);

    // public data
    typedef LIST_TYPE < const std::string * > EnsembleList;
    EnsembleList ensembles;

    // public methods
    void Draw(void) const;
    const Atom* GetAtomPntr(int moleculeID, int residueID, 
                            int atomID, char altConfID = ATOM_NO_ALTCONFID) const;

private:
    // this provides a convenient way to look up atoms from Atom-pntr info
    typedef std::pair < int, std::pair < int, std::pair < int, char > > > AtomPntrKey;
    AtomPntrKey MakeKey(int moleculeID, int residueID, int atomID, char altID) const
    {
        return std::make_pair(moleculeID, std::make_pair(residueID, std::make_pair(atomID, altID))); 
    }
    typedef std::map < AtomPntrKey, const Atom * > AtomMap;
    AtomMap atomMap;

public:
    bool HasTemp(void) const { return (atomMap.size()>0 && (*(atomMap.begin())).second->HasTemp()); }
    bool HasOccup(void) const { return (atomMap.size()>0 && (*(atomMap.begin())).second->HasOccup()); }
    bool HasAlt(void) const { return (atomMap.size()>0 && (*(atomMap.begin())).second->HasAlt()); }
};

END_SCOPE(Cn3D)

#endif // CN3D_ATOMSET__HPP
