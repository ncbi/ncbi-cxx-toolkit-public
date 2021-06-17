#ifndef TYPEMAP__HPP
#define TYPEMAP__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>


/** @addtogroup TypeLookup
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CTypeInfoMapData;
class CTypeInfoMap2Data;

class NCBI_XSERIAL_EXPORT CTypeInfoMap
{
public:
    CTypeInfoMap(void);
    ~CTypeInfoMap(void);

    TTypeInfo GetTypeInfo(TTypeInfo key, TTypeInfoGetter1 func);
    TTypeInfo GetTypeInfo(TTypeInfo key, TTypeInfoCreator1 func);

private:
    CTypeInfoMapData* m_Data;
};

class NCBI_XSERIAL_EXPORT CTypeInfoMap2
{
public:
    CTypeInfoMap2(void);
    ~CTypeInfoMap2(void);

    TTypeInfo GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                          TTypeInfoGetter2 func);
    TTypeInfo GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                          TTypeInfoCreator2 func);

private:
    CTypeInfoMap2Data* m_Data;
};

END_NCBI_SCOPE

#endif  /* TYPEMAP__HPP */


/* @} */
