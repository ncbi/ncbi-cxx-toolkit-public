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
 * Author:  Greg Boratyn
 *
 */

/** @file blast_asn1_input.cpp
 * Convert ASN1-formatted files into blast sequence input
 */

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>

#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blastinput/blast_asn1_input.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


CASN1InputSourceOMF::CASN1InputSourceOMF(CNcbiIstream& infile,
                                         bool is_bin,
                                         bool is_paired)
    : m_InputStream(&infile),
      m_SecondInputStream(NULL),
      m_IsPaired(is_paired),
      m_IsBinary(is_bin)
{}

CASN1InputSourceOMF::CASN1InputSourceOMF(CNcbiIstream& infile1,
                                         CNcbiIstream& infile2,
                                         bool is_bin)
    : m_InputStream(&infile1),
      m_SecondInputStream(&infile2),
      m_IsPaired(true),
      m_IsBinary(is_bin)
{}


int
CASN1InputSourceOMF::GetNextSequence(CBioseq_set& bioseq_set)
{
    m_BasesAdded = 0;

    if (m_SecondInputStream) {
        x_ReadFromTwoFiles(bioseq_set);
    }
    else {
        x_ReadFromSingleFile(bioseq_set);
    }

    return m_BasesAdded;
}


CRef<CSeq_entry>
CASN1InputSourceOMF::x_ReadOneSeq(CNcbiIstream& instream)
{
    CTempString line;
    CTempString id;
    CRef<CSeq_entry> retval;

    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    try {
        if (m_IsBinary) {
            instream >> MSerial_AsnBinary >> *seq_entry;
        }
        else {
            instream >> MSerial_AsnText >> *seq_entry;
        }
    }
    catch (...) {
        if (instream.eof()) {
            return retval;
        }

        NCBI_THROW(CInputException, eInvalidInput, "Problem reading ASN1 entry");
    }

    retval = seq_entry;
    if (!seq_entry->GetSeq().GetInst().IsSetLength()) {
        string message = "Sequence length not set";
        if (seq_entry->GetSeq().GetFirstId()) {
            message += (string)" in the instance of " +
                seq_entry->GetSeq().GetFirstId()->GetSeqIdString();
        }
        NCBI_THROW(CInputException, eInvalidInput, message);
    }
    m_BasesAdded += seq_entry->GetSeq().GetInst().GetLength();

    return retval;
}


bool
CASN1InputSourceOMF::x_ReadFromSingleFile(CBioseq_set& bioseq_set)
{
    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeq_entry> first;
    CRef<CSeq_entry> second;
    first = x_ReadOneSeq(*m_InputStream);

    // if paired rest the next sequence and mark a pair
    if (m_IsPaired) {
        second = x_ReadOneSeq(*m_InputStream);
        
        if (first.NotEmpty()) {
            if (second.NotEmpty()) {
                first->SetSeq().SetDescr().Set().push_back(seqdesc_first);
            }
            bioseq_set.SetSeq_set().push_back(first);
        }

        if (second.NotEmpty()) {
            if (first.NotEmpty()) {
                second->SetSeq().SetDescr().Set().push_back(seqdesc_last);
            }
            bioseq_set.SetSeq_set().push_back(second);
        }
    }
    else {
        // otherwise just add the read sequence
        if (first.NotEmpty()) {
            bioseq_set.SetSeq_set().push_back(first);
        }
    }

    return true;
}


bool
CASN1InputSourceOMF::x_ReadFromTwoFiles(CBioseq_set& bioseq_set)
{
    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeq_entry> first = x_ReadOneSeq(*m_InputStream); 
    CRef<CSeq_entry> second = x_ReadOneSeq(*m_SecondInputStream);

    // if both sequences were read, the pair in the first one
    if (first.NotEmpty() && second.NotEmpty()) {
        first->SetSeq().SetDescr().Set().push_back(seqdesc_first);
        second->SetSeq().SetDescr().Set().push_back(seqdesc_last);

        bioseq_set.SetSeq_set().push_back(first);
        bioseq_set.SetSeq_set().push_back(second);
    }
    else {
        // otherwise mark incomplete pair for the sequence that was read
        if (first.NotEmpty()) {
            bioseq_set.SetSeq_set().push_back(first);
        }

        if (second.NotEmpty()) {
            bioseq_set.SetSeq_set().push_back(second);
        }
    }

    return true;
}


END_SCOPE(blast)
END_NCBI_SCOPE
