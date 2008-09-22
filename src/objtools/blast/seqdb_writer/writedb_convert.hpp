#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_CONVERT_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_CONVERT_HPP

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

/// @file writedb_convert.hpp
/// Data conversion tools for CWriteDB and associated code.
///
/// Defines classes:
///     CAmbiguousRegion
///
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>
#include <objects/blastdb/blastdb__.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// Build blast db protein format from Stdaa protein Seq-inst.
///
/// No conversion is actually done here, because this is already the
/// correct format for disk.  Instead the sequence data is just copied
/// from the Seq-inst to the string.
///
/// @param si Seq-inst containing data in NcbiStdaa format. [in]
/// @param seq Sequence in blast db disk format. [out]
void WriteDB_StdaaToBinary(const CSeq_inst & si, string & seq);

/// Build blast db protein format from Eaa protein Seq-inst.
///
/// The data is converted and returned in the string.
///
/// @param si Seq-inst containing data in NcbiEaa format. [in]
/// @param seq Sequence in blast db disk format. [out]
void WriteDB_EaaToBinary(const CSeq_inst & si, string & seq);

/// Build blast db protein format from Iupacaa protein Seq-inst.
///
/// The data is converted and returned in the string.
///
/// @param si Seq-inst containing data in Iupacaa format. [in]
/// @param seq Sequence in blast db disk format. [out]
void WriteDB_IupacaaToBinary(const CSeq_inst & si, string & seq);

/// Build blast db nucleotide format from Ncbi2na Seq-inst.
///
/// The data is in the correct format, and can be copied as-is, but
/// the length remainder must be coded into the last byte.  It is not
/// necessary to deal with ambiguities - if there were any, ncbi2na
/// would not be the input format.
///
/// @param si Seq-inst containing data in Iupacaa format. [in]
/// @param seq Sequence in blast db disk format. [out]
void WriteDB_Ncbi2naToBinary(const CSeq_inst & si, string & seq);

/// Build blast db nucleotide format from Ncbi4na Seq-inst.
///
/// The data is compressed to ncbi2na, the length remainder is coded
/// into the last byte, and ambiguous region data is produced.
///
/// @param si Seq-inst containing data in Ncbi4na format. [in]
/// @param seq Sequence in blast db disk format. [out]
/// @param amb Ambiguities in blast db disk format. [out]
void WriteDB_Ncbi4naToBinary(const CSeq_inst & seqinst,
                             string          & seq,
                             string          & amb);

/// Build binary blast2na + ambig encoding based on ncbi4na input.
/// 
/// @param ncbi4na Input data with possible ambiguities.
/// @param byte_length Number of bytes in the input data.
/// @param base_length Valid nucleotide bases in the input data.
/// @param seq Sequence data in blast db format.
/// @param amb Ambiguity data in blast db format.


/// Build blast db nucleotide format from Ncbi4na data in memory.
///
/// For a given sequence in ncbi4na format, the blast database format
/// data is constructed; this consists of ncbi2na format with values
/// in ambiguous locations selected randomly, plus the precise values
/// of the ambiguous regions encoded in a seperate string.
///
/// @param ncbi4na Pointer to Ncbi4na format sequence data. [in]
/// @param byte_length Length of ncbi4na data in bytes. [in]
/// @param base_length Number of letters of valid data. [in]
/// @param seq Sequence in blast db disk format. [out]
/// @param seq Ambiguities in blast db disk format. [out]
void WriteDB_Ncbi4naToBinary(const char * ncbi4na,
                             int          byte_length,
                             int          base_length,
                             string     & seq,
                             string     & amb);

/// Append a value to a string as a 4 byte big-endian integer.
/// @param x Value to append.
/// @param outp String to modify.
inline void s_AppendInt4(string & outp, int x)
{
    char buf[4];
    buf[0] = (x >> 24) & 0xFF;
    buf[1] = (x >> 16) & 0xFF;
    buf[2] = (x >> 8)  & 0xFF;
    buf[3] = x         & 0xFF;
    
    outp.append(buf, 4);
}

/// Write a four byte integer to a stream in big endian format.
/// @param str Stream to write to.
/// @param x Integer to write.
inline void s_WriteInt4(ostream & str, int x)
{
    char buf[4];
    buf[0] = (x >> 24) & 0xFF;
    buf[1] = (x >> 16) & 0xFF;
    buf[2] = (x >> 8)  & 0xFF;
    buf[3] = x         & 0xFF;
    
    str.write(buf, 4);
}

/// Write an eight byte integer to a stream in little-endian format.
/// @param str Stream to write to.
/// @param x Integer to write.
inline void s_WriteInt8LE(ostream & str, Uint8 x)
{
    char buf[8];
    buf[7] = (char)((x >> 56) & 0xFF);
    buf[6] = (char)((x >> 48) & 0xFF);
    buf[5] = (char)((x >> 40) & 0xFF);
    buf[4] = (char)((x >> 32) & 0xFF);
    buf[3] = (char)((x >> 24) & 0xFF);
    buf[2] = (char)((x >> 16) & 0xFF);
    buf[1] = (char)((x >> 8)  & 0xFF);
    buf[0] = (char)((x     )  & 0xFF);
    
    str.write(buf, 8);
}

/// Write an eight byte integer to a stream in big-endian format.
/// @param str Stream to write to.
/// @param x Integer to write.
inline void s_WriteInt8BE(ostream & str, Uint8 x)
{
    char buf[8];
    buf[0] = (char)((x >> 56) & 0xFF);
    buf[1] = (char)((x >> 48) & 0xFF);
    buf[2] = (char)((x >> 40) & 0xFF);
    buf[3] = (char)((x >> 32) & 0xFF);
    buf[4] = (char)((x >> 24) & 0xFF);
    buf[5] = (char)((x >> 16) & 0xFF);
    buf[6] = (char)((x >>  8) & 0xFF);
    buf[7] = (char)((x      ) & 0xFF);
    
    str.write(buf, 8);
}

/// Write a length-prefixed string to a stream.
///
/// This method writes a string to a stream, prefixing the string with
/// it's length, written as a big-endian four byte integer.
///
/// @param str Stream to write to.
/// @param s String to write.
inline void s_WriteString(ostream & str, const string & s)
{
    s_WriteInt4(str, (int)s.length());
    str.write(s.data(), s.length());
}

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP

