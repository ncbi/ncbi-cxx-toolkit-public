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
 *   CWinMaskSeqTitle class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include "win_mask_seq_title.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//-------------------------------------------------------------------------
const string CWinMaskSeqTitle::GetTitle( CSeq_entry_Handle & seh, 
                                         const CBioseq & seq )
{
    list< CRef< CSeq_id > > idlist = seq.GetId();
    string title, result( "UNKNOWN" );

    CBioseq_CI it(seh);
    CSeqdesc_CI descit( *it, CSeqdesc::e_Title );

    if( descit )
        title = descit->GetTitle();

    if( !idlist.empty() )
        result = (*idlist.begin())->GetSeqIdString() + " " + title;

    return result;
}

//-------------------------------------------------------------------------
const string CWinMaskSeqTitle::GetId( CSeq_entry_Handle & seh, 
                                      const CBioseq & seq )
{
    string title( GetTitle( seh, seq ) );
    string::size_type pos = title.find_first_of( " \t" );
    return title.substr( 0, pos );
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

