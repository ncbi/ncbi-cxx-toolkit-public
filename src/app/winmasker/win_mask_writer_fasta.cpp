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
 *   CWinMaskWriterFasta class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>

#include "win_mask_writer_fasta.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
void CWinMaskWriterFasta::Print( CBioseq_Handle& bsh,
                                 const CSeqMasker::TMaskList & mask )
{
    PrintId( bsh );
    CSeqVector data = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    /// FIXME: this can be implemented as a call to CFastaOstream, which
    /// understands masking via a seq-loc

    // if( dest->GetIupacna().CanGet() )
    if( true )
    {
        string accumulator;
        CSeqMasker::TMaskList::const_iterator imask = mask.begin();

        for( TSeqPos i = 0; i < data.size(); ++i )
        {
            char letter = data[i];

            if( imask != mask.end() && i >= imask->first )
                if( i <= imask->second ) 
                    letter = tolower( letter );
                else
                {
                    ++imask;

                    if(    imask != mask.end() 
                        && i >= imask->first && i <= imask->second )
                        letter = tolower( letter );
                }

            accumulator.append( 1, letter );

            if( !((i + 1)%60) )
            {
                os << accumulator << "\n";
                accumulator = "";
            }
        }

        if( accumulator.length() ) os << accumulator << "\n";
    }
}


END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/04/06 15:57:10  morgulis
 * Fix in the output fasta formatter for skipping a base in the case of two
 * adjacent masked intervals.
 * Fix in the interval merging code for not merging adjacent intervals.
 *
 * Revision 1.2  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.1  2005/02/25 21:32:55  dicuccio
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

