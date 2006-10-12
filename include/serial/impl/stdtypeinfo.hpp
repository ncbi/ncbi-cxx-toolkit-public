#ifndef STDTYPEINFO__HPP
#define STDTYPEINFO__HPP

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
#include <typeinfo>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// this structure is used for sorting C++ standard type_info class by pointer
struct CLessTypeInfo
{
    // to avoid warning under MSVS, where type_info::before() erroneously
    // returns int, we'll define overloaded functions:
    static bool ToBool(bool b)
        {
            return b;
        }
    static bool ToBool(int i)
        {
            return i != 0;
        }

    bool operator()(const type_info* i1, const type_info* i2) const
        {
            return ToBool(i1->before(*i2));
        }
};


/* @} */


END_NCBI_SCOPE

#endif  /* STDTYPEINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2006/10/12 15:07:28  gouriano
* Some header files moved into impl
*
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.4  2003/04/15 16:18:56  siyan
* Added doxygen support
*
* Revision 1.3  2003/03/26 16:13:33  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.2  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.1  2000/09/18 20:00:10  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
