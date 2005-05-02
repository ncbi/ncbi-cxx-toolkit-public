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
 *   Implementation of CSeqMaskerOStatOptAscii class.
 *
 */

#include <ncbi_pch.hpp>

#include "algo/winmask/seq_masker_ostat_opt_ascii.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CSeqMaskerOstatOptAscii::CSeqMaskerOstatOptAscii( const string & name, 
                                                  Uint2 sz )
    : CSeqMaskerOstatOpt( static_cast< CNcbiOstream& >(
        *new CNcbiOfstream( name.c_str() ) ), sz ) 
{ 
    // File format identifier
    out_stream << (char)65; 
    out_stream << (char)65; 
    out_stream << (char)65; 
    out_stream << (char)65 << endl; 
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatOptAscii::write_out( const params & p ) const
{
    out_stream << (Uint4)UnitSize() << "\n";
    out_stream << p.M << " "
               << (Uint4)p.k << " " << (Uint4)p.roff << " " << (Uint4)p.bc
               << "\n";

    for( Uint4 i = 0; i < GetParams().size(); ++i )
        out_stream << GetParams()[i] << "\n";

    for( Uint4 i = 0, sz = (1<<p.k); i < sz; ++i )
        out_stream << p.ht[i] << "\n";

    for( Uint4 i = 0; i < p.M; ++i )
        out_stream << (Uint4)(p.vt[i]) << "\n";

    out_stream << flush;
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
