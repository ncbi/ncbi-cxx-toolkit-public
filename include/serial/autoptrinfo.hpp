#ifndef AUTOPTRINFO__HPP
#define AUTOPTRINFO__HPP

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

#include <serial/ptrinfo.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CAutoPointerTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    CAutoPointerTypeInfo(TTypeInfo type);

    const string& GetModuleName(void) const;

    static TTypeInfo GetTypeInfo(TTypeInfo base);
    static CTypeInfo* CreateTypeInfo(TTypeInfo base);

protected:
    static void ReadAutoPtr(CObjectIStream& in,
                            TTypeInfo objectType,
                            TObjectPtr objectPtr);
    static void WriteAutoPtr(CObjectOStream& out,
                             TTypeInfo objectType,
                             TConstObjectPtr objectPtr);
    static void SkipAutoPtr(CObjectIStream& in,
                            TTypeInfo objectType);
    static void CopyAutoPtr(CObjectStreamCopier& copier,
                            TTypeInfo objectType);
};

//#include <serial/impl/autoptrinfo.inl>

END_NCBI_SCOPE


/* @} */


#endif  /* AUTOPTRINFO__HPP */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2006/10/05 19:23:03  gouriano
* Some headers moved into impl
*
* Revision 1.12  2003/04/15 14:14:52  siyan
* Added doxygen support
*
* Revision 1.11  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.10  2000/11/07 17:25:11  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.9  2000/10/13 16:28:29  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.8  2000/09/18 19:59:59  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.7  2000/09/01 13:15:57  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.6  2000/03/07 14:05:27  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.5  1999/12/28 18:55:39  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.4  1999/12/17 19:04:51  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.2  1999/09/14 18:54:02  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:42  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/
