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

#include <serial/serial.hpp>            
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb2/Atomic_coordinates.hpp>
USING_NCBI_SCOPE;
using namespace objects;

#include "cn3d/structure_base.hpp"
#include "cn3d/vector_math.hpp"

BEGIN_SCOPE(Cn3D)

class StructureObject;

class StructureSet : public StructureBase
{
public:
    StructureSet(const CNcbi_mime_asn1& mime);
    ~StructureSet(void);

    // public data
    typedef LIST_TYPE < const StructureObject * > ObjectList;
    ObjectList objects;

    // public methods
    void Draw(void) const;

private:
    StructureSet(void);
};

class AtomSet;

class StructureObject : public StructureBase
{
public:
    StructureObject(const CBiostruc& biostruc, bool master);
    ~StructureObject(void);

    // public data
    const bool isMaster;
    int mmdbId;
    string pdbId;
    typedef LIST_TYPE < const AtomSet * > AtomSetList;
    AtomSetList atomSets;

    // public methods
    void Draw(void) const;

private:
    StructureObject(void);
};

class Atom;

class AtomSet : public StructureBase
{
public:
    AtomSet(const CAtomic_coordinates& coords);
    ~AtomSet(void);

    // public data
    typedef LIST_TYPE < const string * > EnsembleList;
    EnsembleList ensembles;

    // public methods
    void Draw(void) const;
    const Atom* GetAtomPntr(int moleculeID, int residueID, int atomID) const;

private:
    AtomSet(void);

    int nAtoms;
    int *moleculeID;
    int *residueID;
    int *atomID;
    Atom *atomList;
};

#define ATOM_NO_TEMPERATURE (-1.0)
#define ATOM_NO_OCCUPANCY (-1.0)
#define ATOM_NO_ALTCONFID ('-')

class Atom : public StructureBase
{
public:
    Atom(void);
    //~Atom(void);

    // public data
    Vector site;
    double averageTemperature; // average of 6 factors if anisotropic
    double occupancy;
    char altConfID;

    // public methods
    bool HasTemp(void) const { return (averageTemperature==ATOM_NO_TEMPERATURE); }
    bool HasOccup(void) const { return (occupancy==ATOM_NO_OCCUPANCY); }
    bool HasAlt(void) const { return (altConfID==ATOM_NO_ALTCONFID); }
    //void Draw(void) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP
