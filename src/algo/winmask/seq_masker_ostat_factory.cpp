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
 *   Implementation of CSeqMaskerUStatFactory class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_ostat_factory.hpp>
#include <algo/winmask/seq_masker_ostat_ascii.hpp>
#include <algo/winmask/seq_masker_ostat_bin.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CSeqMaskerOstatFactory::CSeqMaskerOstatFactoryException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eBadName:      return "bad name";
        case eCreateFail:   return "creation failure";
        default:            return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CSeqMaskerOstat * CSeqMaskerOstatFactory::create( 
    const string & ustat_type, const string & name )
{
    try
    {
        if( ustat_type == "ascii" )
            return new CSeqMaskerOstatAscii( name );
        else if( ustat_type == "binary" )
            return new CSeqMaskerOstatBin( name );
        else NCBI_THROW( CSeqMaskerOstatFactoryException,
                         eBadName,
                         "unkown unit counts format" );
    }
    catch( ... )
    {
        NCBI_THROW( CSeqMaskerOstatFactoryException,
                    eCreateFail,
                    "could not create a unit counts container" );
    }
}
    
END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/04/12 13:35:34  morgulis
 * Support for binary format of unit counts file.
 *
 * Revision 1.2  2005/03/29 13:33:25  dicuccio
 * Use <> for includes
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
