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
*      Class to hold, and factory to generate, general
*      (instance-independent) identifier for any molecule
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/07/04 19:38:55  thiessen
* finish user annotation system
*
* Revision 1.2  2001/06/29 18:12:53  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.1  2001/06/21 02:01:16  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* ===========================================================================
*/

#ifndef CN3D_MOLECULE_IDENTIFIER__HPP
#define CN3D_MOLECULE_IDENTIFIER__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <string>


BEGIN_SCOPE(Cn3D)

class Molecule;
class Sequence;

class MoleculeIdentifier
{
public:
    static const int VALUE_NOT_SET;

    // possible identifiers (not all will necessarily be set)
    int mmdbID, moleculeID, pdbChain, gi;
    std::string pdbID, accession;

    // # residues (1 for non-biopolymers - hets, solvents)
    int nResidues;

    // get title string based on identifiers present
    std::string ToString(void) const;

    // create, or retrieve if known, an identifier for an entity
    static const MoleculeIdentifier * GetIdentifier(const Molecule *molecule,
        std::string pdbID, int pdbChain, int gi, std::string accession);
    static const MoleculeIdentifier * GetIdentifier(const Sequence *sequence,
        std::string pdbID, int pdbChain, int mmdbID, int gi, std::string accession);

    // get identifier for MMDB ID + molecule (NULL if not found)
    static const MoleculeIdentifier * FindIdentifier(int mmdbID, int moleculeID);

    // clear identifier store (e.g. when a new file is loaded)
    static void ClearIdentifiers(void);

    // does this molecule have structure?
    bool HasStructure(void) const
    {
        return (
            (mmdbID != VALUE_NOT_SET && moleculeID != VALUE_NOT_SET) ||
            (pdbID.size() > 0 && pdbChain != VALUE_NOT_SET)
        );
    }

private:
    // can't create one of these directly - must use GetIdentifier()
    MoleculeIdentifier(void) : mmdbID(VALUE_NOT_SET), moleculeID(VALUE_NOT_SET),
        pdbChain(VALUE_NOT_SET), gi(VALUE_NOT_SET), nResidues(0)
        { }

};

END_SCOPE(Cn3D)

#endif // CN3D_MOLECULE_IDENTIFIER__HPP
