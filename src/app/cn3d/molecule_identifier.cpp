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
* Revision 1.1  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* ===========================================================================
*/

#include <corelib/ncbistre.hpp>

#include "cn3d/molecule_identifier.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// there is one (global) list of molecule identifiers

typedef std::list < MoleculeIdentifier > MoleculeIdentifierList;
static MoleculeIdentifierList knownIdentifiers;

const int MoleculeIdentifier::VALUE_NOT_SET = -1;

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Molecule *molecule,
    std::string _pdbID, int _pdbChain, int _gi, std::string _accession)
{
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return NULL;

    // see if there's already an identifier that matches this molecule
    MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; i++) {
        if ((object->mmdbID != StructureObject::NO_MMDB_ID && object->mmdbID == i->mmdbID &&
             i->moleculeID != VALUE_NOT_SET && molecule->id == i->moleculeID) ||
            (_pdbID.size() > 0 && _pdbID == i->pdbID &&
             _pdbChain != VALUE_NOT_SET && _pdbChain == i->pdbChain) ||
            (_gi != VALUE_NOT_SET && _gi == i->gi) ||
            (_accession.size() > 0 && _accession == i->accession)) {
            identifier = &(*i);
            break;
        }
    }

    // if no equivalent found, create a new one
    if (!identifier) {
        knownIdentifiers.resize(knownIdentifiers.size() + 1, MoleculeIdentifier());
        identifier = &(knownIdentifiers.back());
    }

    // check consistency, and see if there's some more information we can fill out
    if (object->mmdbID != StructureObject::NO_MMDB_ID) {
        if (identifier->mmdbID != VALUE_NOT_SET &&
            identifier->mmdbID != object->mmdbID && identifier->moleculeID != molecule->id) {
            ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - mmdb/molecule ID mismatch");
        } else {
            identifier->mmdbID = object->mmdbID;
            identifier->moleculeID = molecule->id;
        }
    }
    if (_pdbID.size() > 0) {
        if (identifier->pdbID.size() > 0) {
            if (identifier->pdbID != _pdbID || identifier->pdbChain != _pdbChain)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - pdbID/chain mismatch");
        } else {
            identifier->pdbID = _pdbID;
            identifier->pdbChain = _pdbChain;
        }
    }
    if (_gi != VALUE_NOT_SET) {
        if (identifier->gi != VALUE_NOT_SET) {
            if (identifier->gi != _gi)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - gi mismatch");
        } else {
            identifier->gi = _gi;
        }
    }
    if (_accession.size() > 0) {
        if (identifier->accession.size() > 0) {
            if (identifier->accession != _accession)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - accession mismatch");
        } else {
            identifier->accession = _accession;
        }
    }

    identifier->nResidues = molecule->NResidues();

//    TESTMSG("molecule: identifier " << identifier << " (" << identifier->ToString() << ')');
    return identifier;
}

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Sequence *sequence,
    std::string _pdbID, int _pdbChain, int _mmdbID, int _gi, std::string _accession)
{
    // see if there's already an identifier that matches this sequence
    MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; i++) {
        if ((_pdbID.size() > 0 && _pdbID == i->pdbID &&
             _pdbChain != VALUE_NOT_SET && _pdbChain == i->pdbChain) ||
            (_gi != VALUE_NOT_SET && _gi == i->gi) ||
            (_accession.size() > 0 && _accession == i->accession)) {
            identifier = &(*i);
            break;
        }
    }

    // if no equivalent found, create a new one
    if (!identifier) {
        knownIdentifiers.resize(knownIdentifiers.size() + 1, MoleculeIdentifier());
        identifier = &(knownIdentifiers.back());
    }

    // check consistency, and see if there's some more information we can fill out
    if (_mmdbID != VALUE_NOT_SET) {
        if (identifier->mmdbID != VALUE_NOT_SET) {
            if (identifier->mmdbID != _mmdbID)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - mmdbID mismatch");
        } else {
            identifier->mmdbID = _mmdbID;
        }
    }
    if (_pdbID.size() > 0) {
        if (identifier->pdbID.size() > 0) {
            if (identifier->pdbID != _pdbID || identifier->pdbChain != _pdbChain)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - pdbID/chain mismatch");
        } else {
            identifier->pdbID = _pdbID;
            identifier->pdbChain = _pdbChain;
        }
    }
    if (_gi != VALUE_NOT_SET) {
        if (identifier->gi != VALUE_NOT_SET) {
            if (identifier->gi != _gi)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - gi mismatch");
        } else {
            identifier->gi = _gi;
        }
    }
    if (_accession.size() > 0) {
        if (identifier->accession.size() > 0) {
            if (identifier->accession != _accession)
                ERR_POST(Error << "MoleculeIdentifier::GetIdentifier() - accession mismatch");
        } else {
            identifier->accession = _accession;
        }
    }

    identifier->nResidues = sequence->Length();

//    TESTMSG("sequence: identifier " << identifier << " (" << identifier->ToString() << ')');
    return identifier;
}

std::string MoleculeIdentifier::ToString(void) const
{
    CNcbiOstrstream oss;
    if (pdbID.size() > 0 && pdbChain != VALUE_NOT_SET) {
        oss << pdbID;
        if (pdbChain != ' ') {
            oss <<  '_' << (char) pdbChain;
        }
    } else if (gi != VALUE_NOT_SET)
        oss << "gi " << gi;
    else if (accession.size() > 0)
        oss << accession;
    else if (mmdbID != VALUE_NOT_SET && moleculeID != VALUE_NOT_SET) {
        oss << "mmdb " << mmdbID << " molecule " << moleculeID;
    } else
        oss << '?';
    oss << '\0';
    auto_ptr<char> chars(oss.str());    // frees memory upon function return
    return std::string(oss.str());
}

void MoleculeIdentifier::ClearIdentifiers(void)
{
    knownIdentifiers.clear();
}

END_SCOPE(Cn3D)
