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
 *   Header file for CSeqMaskerUtil class.
 *
 */

#ifndef C_SEQ_MASKER_UTIL_H
#define C_SEQ_MASKER_UTIL_H

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


/**
 **\brief Collection of various support utilities.
 **
 ** This class is used as a namespace for different utility 
 ** functions that can be used by other winmasker classes.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerUtil
{
public:

    /**
     **\brief Count the bits with given value in a given bit pattern.
     **
     **\param mask the bit pattern
     **\param bit_value if 0 then 0-bits will be counted;
     **                 otherwise 1-bits will be counted
     **\return the bit count
     **
     **/
    static Uint1 BitCount( Uint4 mask, Uint1 bit_value = 1 );
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

