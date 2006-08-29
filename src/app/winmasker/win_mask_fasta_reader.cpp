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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   CWinMaskFastaReader class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seq/Bioseq.hpp>

#include "win_mask_fasta_reader.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//-------------------------------------------------------------------------
CRef< CSeq_entry > CWinMaskFastaReader::GetNextSequence()
{
    CStreamLineReader line_reader( input_stream );

    CFastaReader::TFlags flags = 
        CFastaReader::fAssumeNuc |
        CFastaReader::fForceType |
        CFastaReader::fOneSeq    |
        CFastaReader::fAllSeqIds;

    CFastaReader fasta_reader( line_reader, flags );
    CFastaReader fasta_reader_2( 
            line_reader, flags|CFastaReader::fNoParseID );

    while( !input_stream.eof() )
    {
        CRef< CSeq_entry > aSeqEntry( null );
        CT_POS_TYPE pos = input_stream.tellg();

        try {
            aSeqEntry = fasta_reader.ReadSet( 1 );
        }catch( ... ) {
            input_stream.seekg( pos );
            aSeqEntry = fasta_reader_2.ReadSet( 1 );
        }

        if( aSeqEntry != 0 && 
                aSeqEntry->IsSeq() && 
                aSeqEntry->GetSeq().IsNa() )
            return aSeqEntry;
    }

    return CRef< CSeq_entry >( null );
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.5  2006/08/29 20:02:52  morgulis
 * Changed ReadFasta() to CFastaReader
 *
 * Revision 1.4  2005/10/25 20:11:51  ucko
 * Use CT_POS_TYPE for portability to GCC 2.95.
 *
 * Revision 1.3  2005/10/25 14:03:22  morgulis
 * Worked around the problem with exception being thrown by ReadFasta when defline
 * parsing failed.
 *
 * Revision 1.2  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

