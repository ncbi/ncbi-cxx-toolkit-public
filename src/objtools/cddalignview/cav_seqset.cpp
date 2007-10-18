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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/seq/Bioseq.hpp>
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
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/PDB_block.hpp>

#include <memory>

#include <objtools/cddalignview/cav_seqset.hpp>
#include <objtools/cddalignview/cddalignview.h>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_CAV_Seqset


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void SequenceSet::UnpackSeqSet(const CBioseq_set& bss)
{
    CBioseq_set::TSeq_set::const_iterator q, qe = bss.GetSeq_set().end();
    for (q=bss.GetSeq_set().begin(); q!=qe; ++q) {
        if (q->GetObject().IsSeq()) {

            // only store amino acid or nucleotide sequences
            if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_dna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_rna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_na)
                continue;

            const Sequence *sequence = new Sequence(q->GetObject().GetSeq());
            if (!sequence || sequence->Status() != CAV_SUCCESS) {
                status = sequence->Status();
                return;
            }
            sequences.push_back(sequence);

        } else { // Bioseq-set
            UnpackSeqSet(q->GetObject().GetSet());
        }
    }
}

void SequenceSet::UnpackSeqEntry(const CSeq_entry& seqEntry)
{
    if (seqEntry.IsSeq()) {
        const Sequence *sequence = new Sequence(seqEntry.GetSeq());
        if (!sequence || sequence->Status() != CAV_SUCCESS) {
            status = sequence->Status();
            return;
        }
        sequences.push_back(sequence);
    } else { // Bioseq-set
        UnpackSeqSet(seqEntry.GetSet());
    }
}

SequenceSet::SequenceSet(const CSeq_entry& seqEntry) :
    master(NULL), status(CAV_SUCCESS)
{
    UnpackSeqEntry(seqEntry);
    ERR_POST_X(1, Info << "number of sequences: " << sequences.size());
}

SequenceSet::SequenceSet(const SeqEntryList& seqEntries) :
    master(NULL), status(CAV_SUCCESS)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; ++s)
        UnpackSeqEntry(s->GetObject());
    ERR_POST_X(2, Info << "number of sequences: " << sequences.size());
}

SequenceSet::~SequenceSet(void)
{
    SequenceList::iterator s, se = sequences.end();
    for (s=sequences.begin(); s!=se; ++s) delete *s;
}


const int Sequence::NOT_SET = -1;

#define FIRSTOF2(byte) (((byte) & 0xF0) >> 4)
#define SECONDOF2(byte) ((byte) & 0x0F)

static void StringFrom4na(const vector< char >& vec, string *str, bool isDNA)
{
    if (SECONDOF2(vec.back()) > 0)
        str->resize(vec.size() * 2);
    else
        str->resize(vec.size() * 2 - 1);

    // first, extract 4-bit values
    unsigned int i;
    for (i=0; i<vec.size(); ++i) {
        str->at(2*i) = FIRSTOF2(vec[i]);
        if (SECONDOF2(vec[i]) > 0) str->at(2*i + 1) = SECONDOF2(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); ++i) {
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

static void StringFrom2na(const vector< char >& vec, string *str, bool isDNA)
{
    str->resize(vec.size() * 4);

    // first, extract 4-bit values
    unsigned int i;
    for (i=0; i<vec.size(); ++i) {
        str->at(4*i) = FIRSTOF4(vec[i]);
        str->at(4*i + 1) = SECONDOF4(vec[i]);
        str->at(4*i + 2) = THIRDOF4(vec[i]);
        str->at(4*i + 3) = FOURTHOF4(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); ++i) {
        switch (str->at(i)) {
            case 0: str->at(i) = 'A'; break;
            case 1: str->at(i) = 'C'; break;
            case 2: str->at(i) = 'G'; break;
            case 3: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
        }
    }
}

static void StringFromStdaa(const vector < char >& vec, std::string *str)
{
    static const char *stdaaMap = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

    str->resize(vec.size());
    for (unsigned int i=0; i<vec.size(); ++i)
        str->at(i) = stdaaMap[vec[i]];
}

Sequence::Sequence(const CBioseq& bioseq) :
    status(CAV_ERROR_SEQUENCES), bioseqASN(&bioseq), seqIDs(bioseq.GetId()), mmdbLink(NOT_SET)
{
    // try to get description from title or compound
    if (bioseq.IsSetDescr()) {
        CSeq_descr::Tdata::const_iterator d, de = bioseq.GetDescr().Get().end();
        for (d=bioseq.GetDescr().Get().begin(); d!=de; ++d) {
            if (d->GetObject().IsTitle()) {
                description = d->GetObject().GetTitle();
                break;
            } else if (d->GetObject().IsPdb() && d->GetObject().GetPdb().GetCompound().size() > 0) {
                description = d->GetObject().GetPdb().GetCompound().front();
                break;
            }
        }
    }

    // get link to MMDB id - mainly for CDD's where Biostrucs have to be loaded separately
    if (bioseq.IsSetAnnot()) {
        CBioseq::TAnnot::const_iterator a, ae = bioseq.GetAnnot().end();
        for (a=bioseq.GetAnnot().begin(); a!=ae; ++a) {
            if (a->GetObject().GetData().IsIds()) {
                CSeq_annot::C_Data::TIds::const_iterator i, ie = a->GetObject().GetData().GetIds().end();
                for (i=a->GetObject().GetData().GetIds().begin(); i!=ie; ++i) {
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
        ERR_POST_X(3, Info << "sequence " << GetTitle() << " is from MMDB id " << mmdbLink);

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {

        // protein formats
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsNcbistdaa()) {
            StringFromStdaa(bioseq.GetInst().GetSeq_data().GetNcbistdaa().Get(), &sequenceString);
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
            ERR_POST_X(4, Critical << "Sequence::Sequence() - sequence " << GetTitle()
                          << ": confused by sequence string format");
            return;
        }
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERR_POST_X(5, Critical << "Sequence::Sequence() - sequence string length mismatch");
            return;
        }
    } else {
        ERR_POST_X(6, Critical << "Sequence::Sequence() - sequence " << GetTitle()
                      << ": confused by sequence representation");
        return;
    }

    status = CAV_SUCCESS;
}

string Sequence::GetTitle(void) const
{
    if (seqIDs.size() == 0)
        return kEmptyStr;

    SeqIdList::const_iterator s, se = seqIDs.end();
    for (s=seqIDs.begin(); s!=se; ++s) {
        if ((*s)->IsPdb()) {
            string title = (*s)->GetPdb().GetMol();
            if ((char) (*s)->GetPdb().GetChain() != ' ')
                title += string("_") + (char) (*s)->GetPdb().GetChain();
            return title;
        }
    }

    for (s=seqIDs.begin(); s!=se; ++s)
        if ((*s)->IsGi())
            return (string("gi ") + NStr::IntToString((*s)->GetGi()));

    return seqIDs.front()->GetSeqIdString();
}

string Sequence::GetLabel(void) const
{
    if (seqIDs.size() == 0)
        return kEmptyStr;

    string label;
    SeqIdList::const_iterator s, se = seqIDs.end();
    for (s=seqIDs.begin(); s!=se; ++s) {
        if ((*s)->IsPdb()) {
            label = (*s)->GetPdb().GetMol();
            if ((char) (*s)->GetPdb().GetChain() != ' ')
                label += (char) (*s)->GetPdb().GetChain();
            return label;
        }
    }

    for (s=seqIDs.begin(); s!=se; ++s) {
        if ((*s)->IsGi()) {
            (*s)->GetLabel(&label, objects::CSeq_id::eContent, 0);
            return label;
        }
    }

    seqIDs.front()->GetLabel(&label, objects::CSeq_id::eContent, 0);
    return label;
}

bool Sequence::Matches(const CSeq_id& seqID) const
{
    SeqIdList::const_iterator s, se = seqIDs.end();
    for (s=seqIDs.begin(); s!=se; ++s)
        if ((*s)->Match(seqID))
            return true;
    return false;
}

bool Sequence::Matches(const SeqIdList& others) const
{
    SeqIdList::const_iterator o, oe = others.end();
    for (o=others.begin(); o!=oe; ++o)
        if (Matches(**o))
            return true;
    return false;
}

END_NCBI_SCOPE
