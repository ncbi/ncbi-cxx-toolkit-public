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

/** @file blast_test_util.hpp
 * Utilities to develop and debug unit tests for BLAST
 */

#ifndef _BLAST_TEST_UTIL_HPP
#define _BLAST_TEST_UTIL_HPP

#include <string>
#include <exception>
#include <ctime>

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <util/random_gen.hpp>
#include <util/format_guess.hpp>

#include <serial/serial.hpp>

// NewBlast includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/api/blast_exception.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>

// forward declarations
namespace ncbi {
    namespace objects {
        class CSeq_id;
        class CSeq_align_set;
        class CSeqVector;
        class CScope;
        class CObjectManager;
    }
    namespace blast {
        struct SSeqLoc;
    }
}

namespace TestUtil {

std::vector<EBlastProgramType> GetAllBlastProgramTypes();

ncbi::objects::CSeq_id* GenerateRandomSeqid_Gi();

template <class T>
ncbi::CRef<T> ReadObject(const std::string& filename) {
    ncbi::CNcbiIfstream in(filename.c_str()); 
    if ( !in ) {
        throw std::runtime_error("Failed to open " + filename);
    }
    ncbi::CRef<T> retval(new T);

    switch (ncbi::CFormatGuess().Format(in)) {
    case ncbi::CFormatGuess::eTextASN:
        in >> ncbi::MSerial_AsnText >> *retval;
        break;
    case ncbi::CFormatGuess::eBinaryASN:
        in >> ncbi::MSerial_AsnBinary >> *retval;
        break;
    case ncbi::CFormatGuess::eXml:
        in >> ncbi::MSerial_Xml >> *retval;
        break;
    default:
        throw std::runtime_error("Unsupported format");
    }
    return retval;
}

void CheckForBlastSeqSrcErrors(const BlastSeqSrc* seqsrc)
    THROWS((ncbi::blast::CBlastException));

/// Convenience template function to print ASN.1 objects to a new file
template <class T>
void PrintTextAsn1Object(std::string filename, T* obj) {
    std::ofstream out(filename.c_str());
    if ( !out )
        throw std::runtime_error("Could not open " + filename);
    out << ncbi::MSerial_AsnText << *obj;
}

/** Converts bl2seq and blast style seq-align-sets to the seq-align-set format
 * that the new formatter understands (same flat format as C toolkit
 * seq-aligns) */
ncbi::CRef<ncbi::objects::CSeq_align_set>
FlattenSeqAlignSet(const ncbi::objects::CSeq_align_set& sset);

#if 0
/// Assumes that the sas argument is a bl2seq and blast style seq-align-set
void PrintFormattedSeqAlign(std::ostream& out,
                            const ncbi::objects::CSeq_align_set* sas,
                            ncbi::objects::CScope& scope);
#endif

void PrintSequence(const Uint1* seq, ncbi::TSeqPos len, std::ostream& out,
                   bool show_markers = true,
                   ncbi::TSeqPos chars_per_line = 80);
void PrintSequence(const ncbi::objects::CSeqVector& svector, 
                   std::ostream& out, bool show_markers = true,
                   ncbi::TSeqPos chars_per_line = 80);

/// Returns character representation of a residue from ncbistdaa
char GetResidue(unsigned int res);

/// Creates and initializes a BlastQueryInfo structure for a single protein
/// sequence
BlastQueryInfo*
CreateProtQueryInfo(unsigned int query_size);

/// Endianness independent hash function.
///
/// This function computes a hash value for an array of any primitive
/// type.  The hash assumes the data is the array is in "host" order
/// with respect to endianness and should produce the same value on
/// any platform for the same numerical values of the array
/// elements.<P>
///
/// The algorithm attempts to be robust against changes in values in
/// the array, the length of the array, zeroes appended to the array),
/// and will not normally be fooled by naturally occurring patterns in
/// the buffer.  9However, it is not intended to be secure against
/// deliberate attempts to produce a collision).<P>
///
/// The size of an element of the array must be uniform and is
/// specified as an argument to the function.  It must exactly divide
/// the byte length of the array.  If the size element is specified as
/// 1, no swapping will be done.  This can be used to hash a string.
///
/// @param buffer
///     Points to the beginning of the array.
/// @param byte_length
///     The length of the array in bytes.
/// @param swap_size
///     The size of one array element (specify 1 to disable swapping).
/// @param hash_seed.
///     The starting value of the hash.
Uint4 EndianIndependentBufferHash(const char * buffer,
                                  Uint4        byte_length,
                                  Uint4        swap_size = 1,
                                  Uint4        hash_seed = 1);

}

#endif // _BLAST_TEST_UTIL_HPP
