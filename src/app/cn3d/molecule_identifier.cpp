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
* Revision 1.7  2002/02/22 14:24:00  thiessen
* sort sequences in reject dialog ; general identifier comparison
*
* Revision 1.6  2002/01/24 20:08:16  thiessen
* fix local id problem
*
* Revision 1.5  2001/12/12 14:04:14  thiessen
* add missing object headers after object loader change
*
* Revision 1.4  2001/11/27 16:26:08  thiessen
* major update to data management system
*
* Revision 1.3  2001/08/08 02:25:27  thiessen
* add <memory>
*
* Revision 1.2  2001/07/04 19:39:17  thiessen
* finish user annotation system
*
* Revision 1.1  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* ===========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>

#include <memory>

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

    if (identifier->nResidues == 0)
        identifier->nResidues = molecule->NResidues();
    else if (identifier->nResidues != molecule->NResidues())
        ERR_POST(Error << "Length mismatch in molecule identifier " << identifier->ToString());

    if (molecule->IsProtein() || molecule->IsNucleotide())
        TESTMSG("biopolymer molecule: identifier " << identifier << " (" << identifier->ToString() << ')');
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

    if (identifier->nResidues == 0)
        identifier->nResidues = sequence->Length();
    else if (identifier->nResidues != sequence->Length())
        ERR_POST(Error << "Length mismatch in sequence identifier " << identifier->ToString());

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

const MoleculeIdentifier * MoleculeIdentifier::FindIdentifier(int mmdbID, int moleculeID)
{
    const MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::const_iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; i++) {
        if (mmdbID == i->mmdbID && moleculeID == i->moleculeID) {
            identifier = &(*i);
            break;
        }
    }
    return identifier;
}

void MoleculeIdentifier::ClearIdentifiers(void)
{
    knownIdentifiers.clear();
}

bool MoleculeIdentifier::MatchesSeqId(const ncbi::objects::CSeq_id& sid) const
{
    if (sid.IsGi())
        return (sid.GetGi() == gi);

    if (sid.IsPdb()) {
        if (sid.GetPdb().GetMol().Get() == pdbID) {
            if (sid.GetPdb().IsSetChain() && sid.GetPdb().GetChain() != pdbChain)
                return false;
            else
                return true;
        } else
            return false;
    }

    if (sid.IsLocal() && sid.GetLocal().IsStr())
        return (sid.GetLocal().GetStr() == accession || (accession.size() == 0 &&
                    // special case where local accession is actually a PDB identifier + extra stuff
                    sid.GetLocal().GetStr().size() >= 7 && sid.GetLocal().GetStr()[4] == ' ' &&
                    sid.GetLocal().GetStr()[6] == ' ' && isalpha(sid.GetLocal().GetStr()[5]) &&
                    sid.GetLocal().GetStr().substr(0, 4) == pdbID && sid.GetLocal().GetStr()[5] == pdbChain));

    if (sid.IsGenbank() && sid.GetGenbank().IsSetAccession())
        return (sid.GetGenbank().GetAccession() == accession);

    if (sid.IsSwissprot() && sid.GetSwissprot().IsSetAccession())
        return (sid.GetSwissprot().GetAccession() == accession);

    ERR_POST(Error << "MoleculeIdentifier::MatchesSeqId() - can't match this type of Seq-id");
    return false;
}

bool MoleculeIdentifier::CompareIdentifiers(const MoleculeIdentifier *a, const MoleculeIdentifier *b)
{
    // identifier sort - float sequences with PDB id's to the top, then gi's, then accessions
    if (a->pdbID.size() > 0) {
        if (b->pdbID.size() > 0) {
            if (a->pdbID < b->pdbID)
                return true;
            else if (a->pdbID > b->pdbID)
                return false;
            else
                return (a->pdbChain < b->pdbChain);
        } else
            return true;
    }

    else if (a->gi != VALUE_NOT_SET) {
        if (b->pdbID.size() > 0)
            return false;
        else if (b->gi != VALUE_NOT_SET)
            return (a->gi < b->gi);
        else
            return true;
    }

    else if (a->accession.size() > 0) {
        if (b->pdbID.size() > 0 || b->gi != VALUE_NOT_SET)
            return false;
        else if (b->accession.size() > 0)
            return (a->accession < b->accession);
    }

    ERR_POST(Error << "MoleculeIdentifier::CompareIdentifiers() - confused by identifier type");
    return false;
}

END_SCOPE(Cn3D)
