#ifndef OBJTOOLS_READERS___FASTA__HPP
#define OBJTOOLS_READERS___FASTA__HPP

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
* Authors:  Aaron Ucko, NCBI;  Anatoliy Kuznetsov, NCBI.
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   <objmgr/util/sequence.hpp>.)
*
*/

#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EReadFastaFlags {
    fReadFasta_AssumeNuc  = 0x1,  // type to use if no revealing accn found
    fReadFasta_AssumeProt = 0x2,
    fReadFasta_ForceType  = 0x4,  // force type regardless of accession
    fReadFasta_NoParseID  = 0x8,  // treat name as local ID regardless of |s
    fReadFasta_ParseGaps  = 0x10, // make a delta sequence if gaps found
    fReadFasta_OneSeq     = 0x20, // just read the first sequence found
    fReadFasta_AllSeqIds  = 0x40, // read Seq-ids past the first ^A (see note)
    fReadFasta_NoSeqData  = 0x80  // parse the deflines but skip the data
};
typedef int TReadFastaFlags; // binary OR of EReadFastaFlags

// Note on fReadFasta_AllSeqIds: some databases (notably nr) have
// merged identical sequences, stringing their deflines together with
// control-As.  Normally, the reader stops at the first control-A;
// however, this flag makes it parse all the IDs.

// keeps going until EOF or parse error (-> CParseException) unless
// fReadFasta_OneSeq is set
// see also CFastaOstream in <objects/util/sequence.hpp> (-lxobjutil)
NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags = 0,
                           vector<CRef<CSeq_loc> >* lcv = 0);



//////////////////////////////////////////////////////////////////
//
// Class - description of multi-entry FASTA file,
// to keep list of offsets on all molecules in the file.
//
struct SFastaFileMap
{
    struct SFastaEntry
    {
        string  seq_id;        // Sequence Id
        string  description;   // Molecule description
        size_t  stream_offset; // Molecule offset in file
    };

    typedef vector<SFastaEntry>  TMapVector;

    TMapVector   file_map; // vector keeps list of all molecule entries
};

// Function reads input stream (assumed that it is FASTA format) one
// molecule entry after another filling the map structure describing and
// pointing on molecule entries. Fasta map can be used later for quick
// CSeq_entry retrival
void NCBI_XOBJREAD_EXPORT ReadFastaFileMap(SFastaFileMap* fasta_map, 
                                           CNcbiIfstream& input);

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/08/06 19:08:28  ucko
* Slight interface tweak to ReadFasta: report lowercase locations in a
* vector with one entry per Bioseq rather than a consolidated Seq_loc_mix.
*
* Revision 1.1  2003/06/04 17:26:08  ucko
* Split out from Seq_entry.hpp.
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_READERS___FASTA__HPP */
