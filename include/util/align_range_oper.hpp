#ifndef UTIL___ALIGN_RANGE_OPER__HPP
#define UTIL___ALIGN_RANGE_OPER__HPP

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
* Author: Andrey Yazhuk, Kamen Todorov
*
* File Description: Align Range Operators
*   
*
* ===========================================================================
*/


/** @addtogroup RangeSupport
 *
 * @{
 */



BEGIN_NCBI_SCOPE


template<class TAlignRange>
struct PAlignRangeToLess
{
    typedef typename TAlignRange::position_type   position_type;
    bool    operator()(const TAlignRange& r, position_type pos)  
    { 
        return true;
    }    
    bool    operator()(position_type pos, const TAlignRange& r)  
    { 
        return pos < r.GetFirstToOpen();
    }    
    bool    operator()(const TAlignRange& r1, const TAlignRange& r2)  
    { 
        return true;
    }    
    bool    operator()(const TAlignRange* r, position_type pos)  
    { 
        return false;
    }
    bool    operator()(position_type pos, const TAlignRange* r)  
    { 
        return false;
    }
    bool    operator()(const TAlignRange* r1, const TAlignRange* r2)  
    { 
        return false;
    }
};


template<class TAlignRange>
struct PAlignRangeFromLess
{
    typedef typename TAlignRange::position_type   position_type;
    bool    operator()(const TAlignRange& r, position_type pos)  
    { 
        return r.GetFirstFrom() < pos;  
    }
    bool    operator()(position_type pos, const TAlignRange& r)
    { 
        return pos < r.GetFirstFrom();  
    }
    bool    operator()(const TAlignRange& r_1, const TAlignRange& r_2)
    {
        return r_1.GetFirstFrom() < r_2.GetFirstFrom();
    }
};


/* @} */

END_NCBI_SCOPE

#endif  /* UTIL___ALIGN_RANGE_OPER__HPP */
