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
#include <corelib/ncbistl.hpp>

#include <vector>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include "su_sequence_set.hpp"
#include "su_private.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

static void UnpackSeqSet(CBioseq_set& bss, SequenceSet::SequenceList& seqlist)
{
    CBioseq_set::TSeq_set::iterator q, qe = bss.SetSeq_set().end();
    for (q=bss.SetSeq_set().begin(); q!=qe; ++q) {
        if (q->GetObject().IsSeq()) {

            // only store amino acid or nucleotide sequences
            if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_dna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_rna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_na)
                continue;

            CRef < Sequence > sequence(new Sequence(q->GetObject().SetSeq()));
            seqlist.push_back(sequence);

        } else { // Bioseq-set
            UnpackSeqSet(q->GetObject().SetSet(), seqlist);
        }
    }
}

static void UnpackSeqEntry(CSeq_entry& seqEntry, SequenceSet::SequenceList& seqlist)
{
    if (seqEntry.IsSeq()) {
        CRef < Sequence > sequence(new Sequence(seqEntry.SetSeq()));
        seqlist.push_back(sequence);
    } else { // Bioseq-set
        UnpackSeqSet(seqEntry.SetSet(), seqlist);
    }
}

SequenceSet::SequenceSet(SeqEntryList& seqEntries)
{
    SeqEntryList::iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; ++s)
        UnpackSeqEntry(s->GetObject(), m_sequences);

    TRACE_MESSAGE("number of sequences: " << m_sequences.size());
}

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

static void StringFromStdaa(const vector < char >& vec, string *str)
{
    static const char *stdaaMap = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

    str->resize(vec.size());
    for (unsigned int i=0; i<vec.size(); ++i)
        str->at(i) = stdaaMap[vec[i]];
}

Sequence::Sequence(ncbi::objects::CBioseq& bioseq) :
    m_bioseqASN(&bioseq), m_ids(bioseq.GetId()), m_isProtein(false)
{
    // fill out description
    if (bioseq.IsSetDescr()) {
        string defline, taxid;
        CSeq_descr::Tdata::const_iterator d, de = bioseq.GetDescr().Get().end();
        for (d=bioseq.GetDescr().Get().begin(); d!=de; ++d) {

            // get "defline" from title or compound
            if ((*d)->IsTitle()) {              // prefer title over compound
                defline = (*d)->GetTitle();
            } else if (defline.size() == 0 && (*d)->IsPdb() && (*d)->GetPdb().GetCompound().size() > 0) {
                defline = (*d)->GetPdb().GetCompound().front();
            }

            // get taxonomy
            if ((*d)->IsSource()) {
                if ((*d)->GetSource().GetOrg().IsSetTaxname())
                    taxid = (*d)->GetSource().GetOrg().GetTaxname();
                else if ((*d)->GetSource().GetOrg().IsSetCommon())
                    taxid = (*d)->GetSource().GetOrg().GetCommon();
            }
        }
        if (taxid.size() > 0)
            m_description = string("[") + taxid + ']';
        if (defline.size() > 0) {
            if (taxid.size() > 0)
                m_description += ' ';
            m_description += defline;
        }
    }

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {

        // protein formats
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            m_sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
            m_isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            m_sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
            m_isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsNcbistdaa()) {
            StringFromStdaa(bioseq.GetInst().GetSeq_data().GetNcbistdaa().Get(), &m_sequenceString);
            m_isProtein = true;
        }

        // nucleotide formats
        else if (bioseq.GetInst().GetSeq_data().IsIupacna()) {
            m_sequenceString = bioseq.GetInst().GetSeq_data().GetIupacna().Get();
            // convert 'T' to 'U' for RNA
            if (bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
                for (unsigned int i=0; i<m_sequenceString.size(); ++i) {
                    if (m_sequenceString[i] == 'T')
                        m_sequenceString[i] = 'U';
                }
            }
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi4na()) {
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi4na().Get(), &m_sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi8na()) {  // same repr. for non-X as 4na
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi8na().Get(), &m_sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi2na()) {
            StringFrom2na(bioseq.GetInst().GetSeq_data().GetNcbi2na().Get(), &m_sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
            if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() < m_sequenceString.length())
                m_sequenceString.resize(bioseq.GetInst().GetLength());
        }

        else
            THROW_MESSAGE("Sequence::Sequence(): confused by sequence format");

        // check length
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != m_sequenceString.length())
            THROW_MESSAGE("Sequence::Sequence() - sequence string length mismatch");

        // force uppercase
        for (unsigned int i=0; i<m_sequenceString.size(); ++i)
            m_sequenceString[i] = toupper(m_sequenceString[i]);

    } else
        THROW_MESSAGE("Sequence::Sequence(): confused by sequence representation");
}

bool Sequence::MatchesSeqId(const CSeq_id& seqID) const
{
    CBioseq::TId::const_iterator i, ie = m_ids.end();
    for (i=m_ids.begin(); i!=ie; ++i) {
        if (seqID.Match(**i))
            return true;
    }
    return false;
}

END_SCOPE(struct_util)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/
