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
 *   WindowMasker helper functions (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include <vector>

#include "win_mask_util.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//------------------------------------------------------------------------------
bool CWinMaskUtil::consider( CRef< CScope > scope,
                             const CBioseq_Handle & bsh,
                             const set< CSeq_id_Handle > & ids,
                             const set< CSeq_id_Handle > & exclude_ids )
{
    bool result = true;
    vector< CSeq_id_Handle > syns = scope->GetIds( *bsh.GetSeqId() );

    if( !ids.empty() )
    {
        result = false;
        ITERATE( vector< CSeq_id_Handle >, iter, syns )
        {
            if( ids.find( *iter ) != ids.end() )
            {
                result = true;
                break;
            }
        }
    }

    if( !exclude_ids.empty() )
    {
        ITERATE( vector< CSeq_id_Handle >, iter, syns )
        {
            if( exclude_ids.find( *iter ) != ids.end() )
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/03/24 16:50:22  morgulis
 * -ids and -exclude-ids options can be applied in Stage 1 and Stage 2.
 *
 */
