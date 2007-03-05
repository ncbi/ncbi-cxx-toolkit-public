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

#include <objects/seqloc/Seq_id.hpp>
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
USING_SCOPE(objects);

// there is one (global) list of molecule identifiers

typedef list < MoleculeIdentifier > MoleculeIdentifierList;
static MoleculeIdentifierList knownIdentifiers;

const int MoleculeIdentifier::VALUE_NOT_SET = -1;

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Molecule *molecule, const SeqIdList& ids)
{
    // get or create identifer
    MoleculeIdentifier *identifier = GetIdentifier(ids);
    if (!identifier)
        return NULL;

    // check/assign mmdb id
    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return NULL;
    if (object->mmdbID != StructureObject::NO_MMDB_ID) {
        if ((identifier->mmdbID != VALUE_NOT_SET && identifier->mmdbID != object->mmdbID) ||
                (identifier->moleculeID != VALUE_NOT_SET && identifier->moleculeID != molecule->id)) {
            ERRORMSG("MoleculeIdentifier::GetIdentifier() - mmdb/molecule ID mismatch for " << identifier->ToString());
        } else {
            identifier->mmdbID = object->mmdbID;
            identifier->moleculeID = molecule->id;
        }
    }

    return identifier;
}

const MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const Sequence *sequence, int mmdbID, const SeqIdList& ids)
{
    // get or create identifer
    MoleculeIdentifier *identifier = GetIdentifier(ids);
    if (!identifier)
        return NULL;

    // check/assign mmdb id
    if (mmdbID != VALUE_NOT_SET) {
        if (identifier->mmdbID != VALUE_NOT_SET) {
            if (identifier->mmdbID != mmdbID)
                ERRORMSG("MoleculeIdentifier::GetIdentifier() - mmdbID mismatch for " << identifier->ToString());
        } else {
            identifier->mmdbID = mmdbID;
        }
    }

    // check/assign length
    if (identifier->nResidues == 0)
        identifier->nResidues = sequence->Length();
    else if (identifier->nResidues != sequence->Length())
        ERRORMSG("Length mismatch in sequence identifier for " << identifier->ToString());

    return identifier;
}

MoleculeIdentifier * MoleculeIdentifier::GetIdentifier(const SeqIdList& ids)
{
    // first check known identifiers to see if there's a match, and posibly merge in new ids
    MoleculeIdentifierList::iterator k, ke = knownIdentifiers.end();
    for (k=knownIdentifiers.begin(); k!=ke; ++k) {

        // for each known, compare lists of Seq-ids, looking for matches and mismatches
        SeqIdList newIDs;
        vector < string > matches, mismatches;
        bool mismatchGIonly = false;
        SeqIdList::const_iterator o, oe = k->seqIDs.end(), n, ne = ids.end();
        for (n=ids.begin(); n!=ne; ++n) {

            // does the new (incoming) Seq-id (mis)match any old (existing) Seq-id?
            bool foundMatch = false, foundMismatch = false;
            for (o=k->seqIDs.begin(); o!=oe; ++o) {
                switch ((*o)->Compare(**n)) {
                    case CSeq_id::e_DIFF:   // different types, can't compare; do nothing
                        break;
                    case CSeq_id::e_NO:     // same type but different id -> mismatch
                        mismatches.push_back((*o)->GetSeqIdString() + " != " + (*n)->GetSeqIdString());
                        foundMismatch = true;
                        if (mismatches.size() == 1) {
                            if ((*n)->IsGi())
                                mismatchGIonly = true;
                        } else {
                            mismatchGIonly = false;
                        }
                        break;
                    case CSeq_id::e_YES:    // same type and same id -> match
                        matches.push_back((*o)->GetSeqIdString() + " == " + (*n)->GetSeqIdString());
                        foundMatch = true;
                        break;
                   default:
                        ERRORMSG("Problem comparing Seq-ids " << (*o)->GetSeqIdString() << " and " << (*n)->GetSeqIdString());
                        continue;
                }
            }

            // if no match or mismatch is found, this is a potential new id for this known identifier
            if (!foundMatch && !foundMismatch)
                newIDs.push_back(*n);
        }

        // if we have matches and no mismatches, then we've found the identifier; merge in any new ids
        if (matches.size() > 0 && mismatches.size() == 0) {
            if (newIDs.size() > 0)
                k->AddFields(newIDs);
            return &(*k);
        }

        // if we have matches *and* mismatches then there's a problem
        if (matches.size() > 0 && mismatches.size() > 0) {

            // special case: gi (only) is different but something else (presumably an accession) is the same, then
            // warn about possibly outdated gi; don't merge in new ids
            if (mismatchGIonly) {
                ERRORMSG("GetIdentifier(): incoming Seq-id list has a GI mismatch ("
                    << mismatches.front() << ") with sequence " << k->seqIDs.front()->GetSeqIdString()
                    << " but otherwise matches (" << matches.front()
                    << "); please update outdated GI(s) for this sequence");
                return &(*k);
            }

            // otherwise, error
            ERRORMSG("GetIdentifier(): incoming Seq-id list has match(es) ("
                << matches.front() << ") and mismatch(es) ("
                << mismatches.front() << ") with identifier " << k->ToString());
            return NULL;
        }
    }

    // if we get here, then this is a new sequence
    knownIdentifiers.resize(knownIdentifiers.size() + 1, MoleculeIdentifier());
    MoleculeIdentifier *identifier = &(knownIdentifiers.back());
    identifier->AddFields(ids);
    return identifier;
}

void MoleculeIdentifier::AddFields(const SeqIdList& ids)
{
    // save these ids (should already know that the new ids don't overlap any existing ones)
    seqIDs.insert(seqIDs.end(), ids.begin(), ids.end());

    SeqIdList::const_iterator n, ne = ids.end();
    for (n=ids.begin(); n!=ne; ++n) {

        // pdb
        if ((*n)->IsPdb()) {
            if (pdbID.size() == 0 && pdbChain == VALUE_NOT_SET) {
                pdbID = (*n)->GetPdb().GetMol();
                pdbChain = (*n)->GetPdb().GetChain();
            } else {
                ERRORMSG("AddFields: identifier already has pdb ID " << pdbID << "_" << ((char) pdbChain));
            }
        }

        // gi
        else if ((*n)->IsGi()) {
            if (gi == VALUE_NOT_SET)
                gi = (*n)->GetGi();
            else
                ERRORMSG("AddFields(): identifier already has gi " << gi);
        }

        // special case where local accession is actually a PDB identifier + chain + extra stuff,
        // separated by spaces: of the format '1ABC X ...' where X can be a chain alphanum character or space
        else if (pdbID.size() == 0 && pdbChain == VALUE_NOT_SET &&
            (*n)->IsLocal() && (*n)->GetLocal().IsStr() &&
            (*n)->GetLocal().GetStr().size() >= 7 && (*n)->GetLocal().GetStr()[4] == ' ' &&
            (*n)->GetLocal().GetStr()[6] == ' ' &&
            (isalnum((unsigned char) (*n)->GetLocal().GetStr()[5]) || (*n)->GetLocal().GetStr()[5] == ' '))
        {
            pdbID = (*n)->GetLocal().GetStr().substr(0, 4);
            pdbChain = (*n)->GetLocal().GetStr()[5];
        }
    }
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
}

bool MoleculeIdentifier::MatchesSeqId(const ncbi::objects::CSeq_id& sid) const
{
    SeqIdList::const_iterator i, ie = seqIDs.end();
    for (i=seqIDs.begin(); i!=ie; ++i)
        if (sid.Match(**i))
            return true;

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

    else if (b->pdbID.size() > 0 || b->gi != VALUE_NOT_SET)
        return false;

    else if (a->seqIDs.size() > 0 && b->seqIDs.size() > 0)
        return (a->seqIDs.front()->GetSeqIdString() < b->seqIDs.front()->GetSeqIdString());

    ERRORMSG("Don't know how to compare identifiers " << a->ToString() << " and " << b->ToString());
    return false;
}

string MoleculeIdentifier::ToString(void) const
{
    CNcbiOstrstream oss;
    if (pdbID.size() > 0 && pdbChain != VALUE_NOT_SET) {
        oss << pdbID;
        if (pdbChain != ' ') {
            oss <<  '_' << (char) pdbChain;
        }
    } else if (gi != VALUE_NOT_SET) {
        oss << "gi " << gi;
    } else if (mmdbID != VALUE_NOT_SET && moleculeID != VALUE_NOT_SET) {
        oss << "mmdb " << mmdbID << " molecule " << moleculeID;
    } else if (seqIDs.size() > 0) {
        oss << seqIDs.front()->GetSeqIdString();
    } else {
        oss << '?';
    }
    return (string) CNcbiOstrstreamToString(oss);
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
