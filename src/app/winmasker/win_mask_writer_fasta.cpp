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
void CWinMaskWriterFasta::Print( CSeq_entry_Handle & seh, 
                                 const CBioseq & seq,
                                 const CSeqMasker::TMaskList & mask )
{
    PrintId( seh, seq );
    TSeqPos len = seq.GetInst().GetLength();
    const CSeq_data & seqdata = seq.GetInst().GetSeq_data();
    auto_ptr< CSeq_data > dest( new CSeq_data );
    CSeqportUtil::Convert( seqdata, dest.get(), CSeq_data::e_Iupacna, 
                           0, len );

    // if( dest->GetIupacna().CanGet() )
    if( true )
    {
        const string & data = dest->GetIupacna().Get();
        string accumulator;
        CSeqMasker::TMaskList::const_iterator imask = mask.begin();

        for( TSeqPos i = 0; i < len; ++i )
        {
            char letter = data[i];

            if( imask != mask.end() && i >= imask->first )
                if( i <= imask->second ) letter = tolower( letter );
                else ++imask;

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

