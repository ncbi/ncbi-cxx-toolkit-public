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
 *   Implementation of CSeqMaskerUStat class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_ostat.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * CSeqMaskerOstat::CSeqMaskerOstatException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadState:     return "bad state";
        default:            return CException::GetErrCodeString();
    }
}
            
//------------------------------------------------------------------------------
void CSeqMaskerOstat::setUnitSize( Uint1 us )
{
    if( state != start )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set unit size in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetUnitSize( us );
    state = ulen;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::setUnitCount( Uint4 unit, Uint4 count )
{
    if( state != ulen && state != udata )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set unit count data in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetUnitCount( unit, count );
    state = udata;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::setParam( const string & name, Uint4 value )
{
    if( state != udata && state != thres )
    {
        CNcbiOstrstream ostr;
        ostr << "can not set masking parameters in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    doSetParam( name, value );
    state = thres;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstat::finalize()
{
    if( state != udata && state != thres )
    {
        CNcbiOstrstream ostr;
        ostr << "can not finalize data structure in state " << state;
        string s = CNcbiOstrstreamToString(ostr);
        NCBI_THROW( CSeqMaskerOstatException, eBadState, s );
    }

    state = final;
    doFinalize();
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/05/02 14:27:46  morgulis
 * Implemented hash table based unit counts formats.
 *
 * Revision 1.2  2005/03/29 13:33:15  dicuccio
 * Use <> for includes.  Use CNcbiOstrstream instead of raw ostrstream
 *
 * Revision 1.1  2005/03/28 22:41:06  morgulis
 * Moved win_mask_ustat* files to library and renamed them.
 *
 * Revision 1.1  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * ========================================================================
 */
