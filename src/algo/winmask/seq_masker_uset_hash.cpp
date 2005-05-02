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
 *   Implementation for CSeqMaskerUsetHash class.
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>

#include <algo/winmask/seq_masker_uset_hash.hpp>
#include <algo/winmask/seq_masker_util.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * CSeqMaskerUsetHash::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadIndex:   return "bad index";
        default:          return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
void CSeqMaskerUsetHash::add_ht_info( Uint1 arg_k, Uint1 arg_roff, Uint1 arg_bc,
                                      const Uint4 * arg_ht )
{
    k = arg_k;
    roff = arg_roff;
    bc = arg_bc;
    cmask = (1<<bc) - 1;
    ht.reset( arg_ht );
    htp = ht.get();
}

//------------------------------------------------------------------------------
void CSeqMaskerUsetHash::add_vt_info( Uint4 arg_M, const Uint2 * arg_vt )
{ 
    M = arg_M;
    vt.reset( arg_vt ); 
    vtp = vt.get();
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerUsetHash::get_info( Uint4 unit ) const
{
    Uint4 runit = CSeqMaskerUtil::reverse_complement( unit, unit_size );

    if( runit < unit )
        unit = runit;

    pair< Uint4, Uint1 > hash = CSeqMaskerUtil::hash_code( unit, k, roff );
    Uint4 hval = htp[hash.first];
    Uint4 coll = hval&cmask;
    
    if( coll == 0 )
        return 0;
    else if( coll == 1 )
    {
        if( hash.second != (hval>>24) )
            return 0;
        else return (hval>>bc)&0xFFF;
    }
    else
    {
        if( (hval>>bc) + coll > M )
        {
            ostringstream r;
            r << "bad index at key " << hash.first 
              << " : " << htp[hash.first];
            NCBI_THROW( Exception, eBadIndex, r.str() );
        }

        const Uint2 * start = vtp + (hval>>bc);
        const Uint2 * end = start + coll;

        for( ; start < end; ++start )
            if( ((*start)>>9) == hash.second )
                return (*start)&0x1FF;

        return 0;
    }
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/05/02 14:27:46  morgulis
 * Implemented hash table based unit counts formats.
 *
 * ========================================================================
 */
