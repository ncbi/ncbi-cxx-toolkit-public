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
 * Author: Christiam Camacho
 *
 */

/** @file blast_test_util.cpp
 * Utilities to develop and debug unit tests for BLAST
 */

#include <ncbi_pch.hpp>
#include "blast_test_util.hpp"
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitype.h>
#include <util/random_gen.hpp>

// BLAST includes
#include <algo/blast/api/blast_aux.hpp>
#include "blast_objmgr_priv.hpp"

// Serialization includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

// Object manager includes
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

// Object includes
#include <objects/seqalign/Seq_align_set.hpp>

// Formatter includes
#include <objtools/align_format/showalign.hpp>

#include <sstream>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

namespace TestUtil {

objects::CSeq_id* GenerateRandomSeqid_Gi() 
{
    static CRandom random_gen((CRandom::TValue)time(0));
    return new CSeq_id(CSeq_id::e_Gi, random_gen.GetRand(1, 20000000));
}

vector<EBlastProgramType> GetAllBlastProgramTypes()
{
    vector<EBlastProgramType> retval;
    retval.push_back(eBlastTypeBlastp);
    retval.push_back(eBlastTypeBlastn);
    retval.push_back(eBlastTypeBlastx);
    retval.push_back(eBlastTypeTblastn);
    retval.push_back(eBlastTypeTblastx);

    retval.push_back(eBlastTypePsiBlast);
    retval.push_back(eBlastTypePsiTblastn);

    retval.push_back(eBlastTypeRpsBlast);
    retval.push_back(eBlastTypeRpsTblastn);

    retval.push_back(eBlastTypePhiBlastp);
    retval.push_back(eBlastTypePhiBlastn);

    return retval;
}

CRef<CSeq_align_set>
FlattenSeqAlignSet(const CSeq_align_set& sset)
{
    CRef<CSeq_align_set> retval(new CSeq_align_set());

    ITERATE(CSeq_align_set::Tdata, i, sset.Get()) {
        ASSERT((*i)->GetSegs().IsDisc());

        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*i)->GetSegs().GetDisc().Get()) {
            retval->Set().push_back((*hsp_itr));
        }
    }

    return retval;
}

#if 0
void PrintFormattedSeqAlign(ostream& out,
                            const CSeq_align_set* sas,
                            CScope& scope)
{
    ASSERT(sas);

    int align_opt = CDisplaySeqalign::eShowMiddleLine   |
                    CDisplaySeqalign::eShowGi           |
                    CDisplaySeqalign::eShowBlastInfo    |
                    CDisplaySeqalign::eShowBlastStyleId;

    CRef<CSeq_align_set> saset(FlattenSeqAlignSet(*sas));

    CDisplaySeqalign formatter(*saset, scope);
    formatter.SetAlignOption(align_opt);
    formatter.DisplaySeqalign(out);
}
#endif

// Pretty print sequence
void PrintSequence(const Uint1* seq, TSeqPos len, ostream& out,
                   bool show_markers, TSeqPos chars_per_line)
{
    TSeqPos nlines = len/chars_per_line;

    for (TSeqPos line = 0; line < nlines + 1; line++) {

        // print chars_per_line residues/bases
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < len); i++) {
            out << GetResidue(seq[i]);
        }
        out << endl;

        if ( !show_markers )
            continue;

        // print the residue/base markers
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < len); i++) {
            if (i == 0 || ((i%10) == 0)) {
                out << i;
                stringstream ss;
                ss << i;
                TSeqPos marker_length = ss.str().size();
                i += (marker_length-1);
            } else {
                out << " ";
            }
        }
        out << endl;
    }
}

void PrintSequence(const CSeqVector svector, ostream& out,
                   bool show_markers, TSeqPos chars_per_line)
{
    TSeqPos nlines = svector.size()/chars_per_line;

    for (TSeqPos line = 0; line < nlines + 1; line++) {

        // print chars_per_line residues/bases
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < svector.size()); i++) {
            out << GetResidue(svector[i]);
        }
        out << endl;

        if ( !show_markers )
            continue;

        // print the residue/base markers
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < svector.size()); i++) {
            if (i == 0 || ((i%10) == 0)) {
                out << i;
                stringstream ss;
                ss << i;
                TSeqPos marker_length = ss.str().size();
                i += (marker_length-1);
            } else {
                out << " ";
            }
        }
        out << endl;
    }
}

char GetResidue(unsigned int res)
{
    if ( !(res < BLASTAA_SIZE)) {
        std::stringstream ss;
        ss << "TestUtil::GetResidue(): Invalid residue " << res;
        throw std::runtime_error(ss.str());
    }
    return NCBISTDAA_TO_AMINOACID[res];

}

BlastQueryInfo*
CreateProtQueryInfo(unsigned int query_size)
{
    BlastQueryInfo* retval = BlastQueryInfoNew(eBlastTypeBlastp, 1);
    if ( !retval ) {
        return NULL;
    }
    retval->contexts[0].query_length = query_size;
    retval->max_length              = query_size;
    return retval;
}

void CheckForBlastSeqSrcErrors(const BlastSeqSrc* seqsrc)
    THROWS((CBlastException))
{
    if ( !seqsrc ) {
        return;
    }

    char* error_str = BlastSeqSrcGetInitError(seqsrc);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
}

Uint4
EndianIndependentBufferHash(const char * buffer,
                            Uint4        byte_length,
                            Uint4        swap_size,
                            Uint4        hash_seed)
{
    Uint4 hash = hash_seed;
    Uint4 swap_mask = swap_size - 1;
    
    // Check that swapsize is a power of two.
    _ASSERT((swap_size) && (0 == (swap_mask & swap_size)));
    
    // Insure that the byte_length is a multiple of swap_size
    _ASSERT((byte_length & swap_mask) == 0);
    
    Uint1 end_bytes[] = { 0x44, 0x33, 0x22, 0x11 };
    Uint4 end_value = *((int *) & end_bytes);
    
    if (end_value == 0x11223344) {
        // Prevent actual swapping on little endian machinery.
        swap_size = 1;
        swap_mask = 0;
    }
    
    Uint4 keep_mask = ~ swap_mask;
    
    // Logical address is the address if the data was little endian.
    
    for(Uint4 logical = 0; logical < byte_length; logical++) {
        Uint4 physical =
            (logical & keep_mask) | (swap_mask - (logical & swap_mask));
        
        // Alternate addition and XOR.  This technique destroys most
        // of the possible mathematical relationships between similar
        // input strings.
        
        if (logical & 1) {
            hash += int(buffer[physical]) & 0xFF;
        } else {
            hash ^= int(buffer[physical]) & 0xFF;
        }
        
        // 1. "Rotate" by a value relatively prime to 32 (any odd
        //    value), to insure that each input bit will eventually
        //    affect each position.
        // 2. Add a per-iteration constant to detect changes in length.
        
        hash = ((hash << 13) | (hash >> 19)) + 1234;
    }
    
    return hash;
}

}

