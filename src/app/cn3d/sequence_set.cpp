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
*      Classes to hold sets of sequences
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.16  2001/01/09 21:45:00  thiessen
* always use pdbID as title if known
*
* Revision 1.15  2001/01/04 18:20:52  thiessen
* deal with accession seq-id
*
* Revision 1.14  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.13  2000/12/28 20:43:09  vakatov
* Do not use "set" as identifier.
* Do not include <strstream>. Use "CNcbiOstrstream" instead of "ostrstream".
*
* Revision 1.12  2000/12/21 23:42:15  thiessen
* load structures from cdd's
*
* Revision 1.11  2000/12/20 23:47:48  thiessen
* load CDD's
*
* Revision 1.10  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.9  2000/11/17 19:48:13  thiessen
* working show/hide alignment row
*
* Revision 1.8  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.7  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.6  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.5  2000/08/30 23:46:27  thiessen
* working alignment display
*
* Revision 1.4  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.3  2000/08/28 23:47:19  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.2  2000/08/28 18:52:42  thiessen
* start unpacking alignments
*
* Revision 1.1  2000/08/27 18:52:22  thiessen
* extract sequence information
*
* ===========================================================================
*/

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <corelib/ncbistre.hpp>

#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static void UnpackSeqSet(const CBioseq_set& bss, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    CBioseq_set::TSeq_set::const_iterator q, qe = bss.GetSeq_set().end();
    for (q=bss.GetSeq_set().begin(); q!=qe; q++) {
        if (q->GetObject().IsSeq()) {

            // only store amino acid or nucleotide sequences
            if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_dna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_rna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_na)
                continue;

            const Sequence *sequence = new Sequence(parent, q->GetObject().GetSeq());
            seqlist.push_back(sequence);

        } else { // Bioseq-set
            UnpackSeqSet(q->GetObject().GetSet(), parent, seqlist);
        }
    }
}

static void UnpackSeqEntry(const CSeq_entry& seqEntry, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    if (seqEntry.IsSeq()) {
        const Sequence *sequence = new Sequence(parent, seqEntry.GetSeq());
        seqlist.push_back(sequence);
    } else { // Bioseq-set
        UnpackSeqSet(seqEntry.GetSet(), parent, seqlist);
    }
}

SequenceSet::SequenceSet(StructureBase *parent, const CSeq_entry& seqEntry) :
    StructureBase(parent), master(NULL)
{
    UnpackSeqEntry(seqEntry, this, sequences);
    TESTMSG("number of sequences: " << sequences.size());
}

SequenceSet::SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries) :
    StructureBase(parent), master(NULL)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; s++)
        UnpackSeqEntry(s->GetObject(), this, sequences);
    TESTMSG("number of sequences: " << sequences.size());
}

const int Sequence::NOT_SET = -1;

#define FIRSTOF2(byte) (((byte) & 0xF0) >> 4)
#define SECONDOF2(byte) ((byte) & 0x0F)

static void StringFrom4na(const std::vector< char >& vec, std::string *str, bool isDNA)
{
    if (SECONDOF2(vec.back()) > 0)
        str->resize(vec.size() * 2);
    else
        str->resize(vec.size() * 2 - 1);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); i++) {
        str->at(2*i) = FIRSTOF2(vec[i]);
        if (SECONDOF2(vec[i]) > 0) str->at(2*i + 1) = SECONDOF2(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); i++) {
        switch (str->at(i)) {
            case 1: str->at(i) = 'A'; break;
            case 2: str->at(i) = 'C'; break;
            case 4: str->at(i) = 'G'; break;
            case 8: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
            default:
                str->at(i) = 'X';
        }
    }
}

#define FIRSTOF4(byte) (((byte) & 0xC0) >> 6)
#define SECONDOF4(byte) (((byte) & 0x30) >> 4)
#define THIRDOF4(byte) (((byte) & 0x0C) >> 2)
#define FOURTHOF4(byte) ((byte) & 0x03)

static void StringFrom2na(const std::vector< char >& vec, std::string *str, bool isDNA)
{
    str->resize(vec.size() * 4);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); i++) {
        str->at(4*i) = FIRSTOF4(vec[i]);
        str->at(4*i + 1) = SECONDOF4(vec[i]);
        str->at(4*i + 2) = THIRDOF4(vec[i]);
        str->at(4*i + 3) = FOURTHOF4(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); i++) {
        switch (str->at(i)) {
            case 0: str->at(i) = 'A'; break;
            case 1: str->at(i) = 'C'; break;
            case 2: str->at(i) = 'G'; break;
            case 3: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
        }
    }
}

Sequence::Sequence(StructureBase *parent, const ncbi::objects::CBioseq& bioseq) :
    StructureBase(parent), gi(NOT_SET), pdbChain(' '), molecule(NULL), mmdbLink(NOT_SET)
{
    // get Seq-id info
    CBioseq::TId::const_iterator s, se = bioseq.GetId().end();
    for (s=bioseq.GetId().begin(); s!=se; s++) {
        if (s->GetObject().IsGi()) {
            gi = s->GetObject().GetGi();
        } else if (s->GetObject().IsPdb()) {
            pdbID = s->GetObject().GetPdb().GetMol().Get();
            if (s->GetObject().GetPdb().IsSetChain())
                pdbChain = s->GetObject().GetPdb().GetChain();
        } else if (s->GetObject().IsLocal() && s->GetObject().GetLocal().IsStr()) {
            pdbID = s->GetObject().GetLocal().GetStr();
        } else if (s->GetObject().IsGenbank() && s->GetObject().GetGenbank().IsSetAccession()) {
            accession = s->GetObject().GetGenbank().GetAccession();
        }
    }
    if (gi == NOT_SET && pdbID.size() == 0) {
        ERR_POST(Error << "Sequence::Sequence() - can't parse SeqId");
        return;
    }

    // get link to MMDB id - mainly for CDD's where Biostrucs have to be loaded separately
    if (bioseq.IsSetAnnot()) {
        CBioseq::TAnnot::const_iterator a, ae = bioseq.GetAnnot().end();
        for (a=bioseq.GetAnnot().begin(); a!=ae; a++) {
            if (a->GetObject().GetData().IsIds()) {
                CSeq_annot::C_Data::TIds::const_iterator i, ie = a->GetObject().GetData().GetIds().end();
                for (i=a->GetObject().GetData().GetIds().begin(); i!=ie; i++) {
                    if (i->GetObject().IsGeneral() &&
                        i->GetObject().GetGeneral().GetDb() == "mmdb" &&
                        i->GetObject().GetGeneral().GetTag().IsId()) {
                        mmdbLink = i->GetObject().GetGeneral().GetTag().GetId();
                        break;
                    }
                }
                if (i != ie) break;
            }
        }
    }
    if (mmdbLink != NOT_SET)
        TESTMSG("sequence gi " << gi << ", PDB '" << pdbID << "' chain '" << (char) pdbChain <<
            "', is from MMDB id " << mmdbLink);

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {

        // protein formats
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
        }

        // nucleotide formats
        else if (bioseq.GetInst().GetSeq_data().IsIupacna()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacna().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi4na()) {
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi4na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi8na()) {  // same repr. for non-X as 4na
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi8na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi2na()) {
            StringFrom2na(bioseq.GetInst().GetSeq_data().GetNcbi2na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
            if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() < sequenceString.length())
                sequenceString.resize(bioseq.GetInst().GetLength());
        }

        else {
            ERR_POST(Critical << "Sequence::Sequence() - sequence " << gi
                << ": confused by sequence string format");
            return;
        }
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERR_POST(Critical << "Sequence::Sequence() - sequence string length mismatch");
            return;
        }
    } else {
        ERR_POST(Critical << "Sequence::Sequence() - sequence " << gi
                << ": confused by sequence representation");
        return;
    }
}

CSeq_id * Sequence::CreateSeqId(void) const
{
    CSeq_id *sid = new CSeq_id();
    if (pdbID.size() > 0) {
        sid->SetPdb().SetMol().Set(pdbID);
        if (pdbChain != ' ') sid->SetPdb().SetChain(pdbChain);
    } else { // use gi
        sid->SetGi(gi);
    }
    return sid;
}

std::string Sequence::GetTitle(void) const
{
    CNcbiOstrstream oss;
    if (pdbID.size() > 0) {
        oss << pdbID;
        if (pdbChain != ' ') {
            oss <<  '_' << (char) pdbChain;
        }
    } else if (gi != NOT_SET)
        oss << "gi " << gi;
    else if (accession.size() > 0)
        oss << "acc " << accession;
    else
        oss << '?';
    oss << '\0';
    auto_ptr<char> chars(oss.str());    // frees memory upon function return
    return std::string(oss.str());
}

END_SCOPE(Cn3D)
