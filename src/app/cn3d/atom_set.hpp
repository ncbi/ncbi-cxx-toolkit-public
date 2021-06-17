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
* ===========================================================================
*/

#ifndef CN3D_ATOMSET__HPP
#define CN3D_ATOMSET__HPP

#include <corelib/ncbistl.hpp>

#include <string>
#include <map>

#include <objects/mmdb2/Atomic_coordinates.hpp>

#include "structure_base.hpp"
#include "vector_math.hpp"

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
    static const char NO_ALTCONFID;

    // public methods
    bool HasTemp(void) const { return (averageTemperature!=NO_TEMPERATURE); }
    bool HasOccup(void) const { return (occupancy!=NO_OCCUPANCY); }
    bool HasAlt(void) const { return (altConfID!=NO_ALTCONFID); }

private:
};

class StructureSet;

class AtomSet : public StructureBase
{
    friend class StructureSet;
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
