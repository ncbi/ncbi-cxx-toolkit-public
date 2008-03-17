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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbobj.cpp
/// Definitions of various helper functions for SeqDB.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>

#include <objmgr/seq_vector.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);

template<class TSource>
unsigned SeqDB_ComputeSequenceHash(TSource & src)
{
    unsigned retval = 0;
    
    while(src.More()) {
        unsigned seq_i = unsigned(src.Get()) & 0xFF;
        
        retval *= 1103515245;
        retval += seq_i + 12345;
    }
    
    return retval;
}

struct SSeqDB_ArraySource {
    SSeqDB_ArraySource(const char * ptr, int len)
        : begin(ptr), end(ptr + len)
    {
    }
    
    bool More()
    {
        return begin != end;
    }
    
    unsigned char Get()
    {
        return *(begin++);
    }
    
    const char *begin, *end;
};

struct SSeqDB_SVCISource {
    SSeqDB_SVCISource(const CBioseq & bs)
        : index(0), size(0)
    {
        // Note: the CSeqVector API provides eCoding_Ncbi, which is
        // labelled as ncbi4na, but op[] provides one residue or base
        // per byte, which is what the SeqDB and ASN.1 formats refer
        // to as the "ncbi8na" format, which is what we need here.
        
        seqvector = CSeqVector(bs,
                               0,
                               CBioseq_Handle::eCoding_Ncbi,
                               eNa_strand_plus);
        
        size = seqvector.size();
    }
    
    bool More()
    {
        return index < size;
    }
    
    unsigned char Get()
    {
        return seqvector[index++];
    }
    
    CSeqVector seqvector;
    TSeqPos index, size;
};

unsigned SeqDB_SequenceHash(const char * sequence,
                             int          length)
{
    SSeqDB_ArraySource src(sequence, length);
    return SeqDB_ComputeSequenceHash(src);
}

// This could be made public if needed, but for now it should be
// enough to use it from the function following it.

static unsigned SeqDB_SequenceHashSeqVector(const CBioseq & bs)
{
    SSeqDB_SVCISource src(bs);
    return SeqDB_ComputeSequenceHash(src);
}

unsigned SeqDB_SequenceHash(const CBioseq & sequence)
{
    if (! sequence.CanGetInst()) {
        return SeqDB_SequenceHashSeqVector(sequence);
    }
    
    const CSeq_inst & si = sequence.GetInst();
    
    // Either get output_data to point to a target format or gather
    // enough info to call CSeqConvert to produce a target format.
    
    if (! sequence.GetInst().CanGetSeq_data()) {
        return SeqDB_SequenceHashSeqVector(sequence);
    }
    
    bool need_cvt = true;
    bool is_protein = false;
    CSeqUtil::ECoding coding(CSeqUtil::e_not_set);
    CTempString input_data;
    CTempString output_data;
    
    const CSeq_data & sd = si.GetSeq_data();
    
    if (! si.CanGetLength()) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "No sequence length in Bioseq.");
    }
    
    int base_length = si.GetLength();
    
    const vector<char> * vp = 0;
    const string * sp = 0;
    
    switch(sd.Which()) {
        
            // Protein

        case CSeq_data::e_Ncbistdaa:
            need_cvt = false;
            is_protein = true;
            coding = CSeqUtil::e_Ncbistdaa;
            vp = & si.GetSeq_data().GetNcbistdaa().Get();
            output_data.assign(& (*vp)[0], vp->size());
            break;
            
        case CSeq_data::e_Ncbieaa:
            need_cvt = true;
            is_protein = true;
            coding = CSeqUtil::e_Ncbieaa;
            sp = & si.GetSeq_data().GetNcbieaa().Get();
            input_data.assign(sp->data(), sp->size());
            break;
            
        case CSeq_data::e_Iupacaa:
            need_cvt = true;
            is_protein = true;
            coding = CSeqUtil::e_Iupacaa;
            sp = & si.GetSeq_data().GetIupacaa().Get();
            input_data.assign(sp->data(), sp->size());
            break;
            
        case CSeq_data::e_Ncbi8aa:
            need_cvt = true;
            is_protein = true;
            coding = CSeqUtil::e_Ncbi8aa;
            vp = & si.GetSeq_data().GetNcbi8aa().Get();
            input_data.assign(& (*vp)[0], vp->size());
            break;
            
            // Nucleotide

        case CSeq_data::e_Iupacna:
            need_cvt = true;
            is_protein = false;
            coding = CSeqUtil::e_Iupacna;
            sp = & si.GetSeq_data().GetIupacna().Get();
            input_data.assign(sp->data(), sp->size());
            break;
            
        case CSeq_data::e_Ncbi2na:
            need_cvt = true;
            is_protein = false;
            coding = CSeqUtil::e_Ncbi2na;
            vp = & si.GetSeq_data().GetNcbi2na().Get();
            input_data.assign(& (*vp)[0], vp->size());
            break;
            
        case CSeq_data::e_Ncbi4na:
            need_cvt = true;
            is_protein = false;
            coding = CSeqUtil::e_Ncbi4na;
            vp = & si.GetSeq_data().GetNcbi4na().Get();
            input_data.assign(& (*vp)[0], vp->size());
            break;
            
        case CSeq_data::e_Ncbi8na:
            need_cvt = false;
            is_protein = false;
            coding = CSeqUtil::e_Ncbi8na;
            vp = & si.GetSeq_data().GetNcbi8na().Get();
            output_data.assign(& (*vp)[0], vp->size());
            break;
            
        default:
            string msg = "Conversion for CBioseq type [";
            msg += NStr::IntToString((int) sd.Which());
            msg += "] not supported.";
            
            NCBI_THROW(CSeqDBException, eArgErr, msg);
    }
    
    string buffer;
    
    if (need_cvt) {
        CSeqUtil::ECoding target_coding =
            (is_protein
             ? CSeqUtil::e_Ncbistdaa
             : CSeqUtil::e_Ncbi8na);
        
        int amt = CSeqConvert::Convert(input_data,
                                       coding,
                                       0,
                                       base_length,
                                       buffer,
                                       target_coding);
        
        if (amt == 0) {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "Could not do data type conversion for CBioseq.");
        }
        
        output_data.assign(buffer.data(), buffer.size());
    }
    
    return SeqDB_SequenceHash(output_data.data(), output_data.size());
}


END_NCBI_SCOPE

