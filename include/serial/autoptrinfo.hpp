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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/09/14 18:54:02  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:42  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <serial/ptrinfo.hpp>

BEGIN_NCBI_SCOPE

class CAutoPointerTypeInfo : public CPointerTypeInfo {
    typedef CPointerTypeInfo CParent;
public:
    CAutoPointerTypeInfo(TTypeInfo type)
        : CParent(type->GetName(), type)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo base)
        {
            return sm_Map.GetTypeInfo(base);
        }

protected:
    
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    static CTypeInfoMap<CAutoPointerTypeInfo> sm_Map;
};

//#include <serial/autoptrinfo.inl>

END_NCBI_SCOPE

#endif  /* AUTOPTRINFO__HPP */
