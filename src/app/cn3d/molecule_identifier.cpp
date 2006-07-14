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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>

#include "molecule_identifier.hpp"
#include "structure_set.hpp"
#include "molecule.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// there is one (global) list of molecule identifiers

typedef list < MoleculeIdentifier > MoleculeIdentifierList;
static MoleculeIdentifierList knownIdentifiers;
static map < string, bool > bannedAccessions;

const int MoleculeIdentifier::VALUE_NOT_SET = -1;

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Molecule *molecule,
    const string& _pdbID, int _pdbChain, int _gi, const string& _accession)
{
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return NULL;

    // see if there's already an identifier that matches this molecule
    MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; ++i) {
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
            ERRORMSG("MoleculeIdentifier::GetIdentifier() - mmdb/molecule ID mismatch");
        } else {
            identifier->mmdbID = object->mmdbID;
            identifier->moleculeID = molecule->id;
        }
    }
    if (_pdbID.size() > 0) {
        if (identifier->pdbID.size() > 0) {
            if (identifier->pdbID != _pdbID || identifier->pdbChain != _pdbChain)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - pdbID/chain mismatch");
        } else {
            identifier->pdbID = _pdbID;
            identifier->pdbChain = _pdbChain;
        }
    }
    if (_gi != VALUE_NOT_SET) {
        if (identifier->gi != VALUE_NOT_SET) {
            if (identifier->gi != _gi)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - gi mismatch");
        } else {
            identifier->gi = _gi;
        }
    }
    if (_accession.size() > 0) {
        if (identifier->accession.size() > 0) {
            if (identifier->accession != _accession)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - accession mismatch");
        } else {
            if (bannedAccessions.find(_accession) == bannedAccessions.end())
                identifier->accession = _accession;
        }
    }

    if (identifier->nResidues == 0)
        identifier->nResidues = molecule->NResidues();
    else if (identifier->nResidues != molecule->NResidues())
        ERRORMSG("Length mismatch in molecule identifier " << identifier->ToString());

    if (molecule->IsProtein() || molecule->IsNucleotide())
        TRACEMSG("biopolymer molecule: identifier " << identifier << " (" << identifier->ToString() << ')');
    return identifier;
}

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Sequence *sequence,
    const string& _pdbID, int _pdbChain, int _mmdbID, int _gi, const string& _accession)
{
    // see if there's already an identifier that matches this sequence
    MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; ++i) {
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

    if (_gi != VALUE_NOT_SET) {
        if (identifier->gi != VALUE_NOT_SET) {
            if (identifier->gi != _gi) {
                if (identifier->accession.size() > 0 && identifier->accession == _accession) {

                    // special case where accession is same, gi is different: two versions of same sequence
                    ERRORMSG("The sequence gi " << _gi << " has the same accession code ("
                        << _accession << ") as gi " << identifier->gi
                        << ". You should remove the older sequence (presumably gi "
                        << ((_gi < identifier->gi) ? _gi : identifier->gi) << ").");

                    // make a new identifier to keep the two gi's separate, and "forget" the accession
                    // so there's no ambiguity if something tries to refer to one of these by accession only
                    bannedAccessions[_accession] = true;
                    identifier->accession.erase();
                    knownIdentifiers.resize(knownIdentifiers.size() + 1, MoleculeIdentifier());
                    identifier = &(knownIdentifiers.back());
                    identifier->gi = _gi;

                } else {
                    ERRORMSG("MoleculeIdentifier::GetIdentifier() - gi mismatch");
                }
            }
        } else {
            identifier->gi = _gi;
        }
    }

    // check consistency, and see if there's some more information we can fill out
    if (_mmdbID != VALUE_NOT_SET) {
        if (identifier->mmdbID != VALUE_NOT_SET) {
            if (identifier->mmdbID != _mmdbID)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - mmdbID mismatch");
        } else {
            identifier->mmdbID = _mmdbID;
        }
    }
    if (_pdbID.size() > 0) {
        if (identifier->pdbID.size() > 0) {
            if (identifier->pdbID != _pdbID || identifier->pdbChain != _pdbChain)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - pdbID/chain mismatch");
        } else {
            identifier->pdbID = _pdbID;
            identifier->pdbChain = _pdbChain;
        }
    }
    if (_accession.size() > 0) {
        if (identifier->accession.size() > 0) {
            if (identifier->accession != _accession)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - accession mismatch");
        } else {
            if (bannedAccessions.find(_accession) == bannedAccessions.end())
                identifier->accession = _accession;
        }
    }

    if (identifier->nResidues == 0)
        identifier->nResidues = sequence->Length();
    else if (identifier->nResidues != sequence->Length())
        ERRORMSG("Length mismatch in sequence identifier " << identifier->ToString());

//    TESTMSG("sequence: identifier " << identifier << " (" << identifier->ToString() << ')');
    return identifier;
}

string MoleculeIdentifier::ToString(void) const
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
    return (string) CNcbiOstrstreamToString(oss);
}

const MoleculeIdentifier * MoleculeIdentifier::FindIdentifier(int mmdbID, int moleculeID)
{
    const MoleculeIdentifier *identifier = NULL;
    MoleculeIdentifierList::const_iterator i, ie = knownIdentifiers.end();
    for (i=knownIdentifiers.begin(); i!=ie; ++i) {
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
    bannedAccessions.clear();
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

    if (sid.IsLocal()) {
        if (sid.GetLocal().IsStr())
            return (sid.GetLocal().GetStr() == accession || (accession.size() == 0 &&
                    // special case where local accession is actually a PDB identifier + extra stuff
                    sid.GetLocal().GetStr().size() >= 7 && sid.GetLocal().GetStr()[4] == ' ' &&
                    sid.GetLocal().GetStr()[6] == ' ' && isalpha((unsigned char) sid.GetLocal().GetStr()[5]) &&
                    sid.GetLocal().GetStr().substr(0, 4) == pdbID && sid.GetLocal().GetStr()[5] == pdbChain));
        else
            return (NStr::IntToString(sid.GetLocal().GetId()) == accession);
    }

    if (sid.IsGenbank() && sid.GetGenbank().IsSetAccession())
        return (sid.GetGenbank().GetAccession() == accession);

    if (sid.IsSwissprot() && sid.GetSwissprot().IsSetAccession())
        return (sid.GetSwissprot().GetAccession() == accession);

    if (sid.IsOther() && sid.GetOther().IsSetAccession())
        return (sid.GetOther().GetAccession() == accession);

    if (sid.IsEmbl() && sid.GetEmbl().IsSetAccession())
        return (sid.GetEmbl().GetAccession() == accession);

    if (sid.IsDdbj() && sid.GetDdbj().IsSetAccession())
        return (sid.GetDdbj().GetAccession() == accession);

    if (sid.IsPir() && sid.GetPir().IsSetAccession())
        return (sid.GetPir().GetAccession() == accession);

    ERRORMSG("MoleculeIdentifier::MatchesSeqId() - can't match this type of Seq-id");
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

    ERRORMSG("MoleculeIdentifier::CompareIdentifiers() - confused by identifier type");
    return false;
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2006/07/14 21:05:19  thiessen
* handle local integer ids
*
* Revision 1.21  2005/10/28 15:42:34  thiessen
* more graceful handling of same accession multiple gi version conflict
*
* Revision 1.20  2005/10/26 18:36:05  thiessen
* minor fixes
*
* Revision 1.19  2005/10/22 02:50:34  thiessen
* deal with memory issues, mostly in ostrstream->string conversion
*
* Revision 1.18  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.17  2005/06/03 16:26:18  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.16  2004/09/27 23:35:17  thiessen
* don't abort on sequence import if gi/acc mismatch, but only on original load
*
* Revision 1.15  2004/09/27 18:31:00  thiessen
* exit gracefully on same accession different gi
*
* Revision 1.14  2004/09/15 18:34:38  thiessen
* handle pir accessions
*
* Revision 1.13  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.12  2004/03/15 18:25:36  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.11  2004/02/19 17:04:59  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.10  2004/01/05 17:09:15  thiessen
* abort import and warn if same accession different gi
*
* Revision 1.9  2003/09/03 18:14:01  thiessen
* hopefully fix Seq-id issues
*
* Revision 1.8  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
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
*/
