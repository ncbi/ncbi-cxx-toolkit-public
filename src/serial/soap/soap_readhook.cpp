
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
* Author: Andrei Gourianov
*
* File Description:
*   Analyzes and reads SOAP message contents
*       known object types are being read as such
*       unknown ones - as CAnyContentObject
*   
*/

#include <ncbi_pch.hpp>
#include <serial/objistr.hpp>
#include "soap_envelope.hpp"
#include "soap_readhook.hpp"


BEGIN_NCBI_SCOPE

CSoapReadHook::CSoapReadHook(
    vector< CConstRef<CSerialObject> >& content,
    const vector< const CTypeInfo* >& types)
    : m_Content(content), m_Types(types)
{
}


void CSoapReadHook::ReadObject(CObjectIStream& in,
                               const CObjectInfo& /*object*/)
{
    string name = in.PeekNextTypeName();

    const CTypeInfo* typeInfo = x_FindType(name);
    if (!typeInfo) {
        typeInfo = CAnyContentObject::GetTypeInfo();
    }

    CObjectInfo info(typeInfo->Create(), typeInfo);
    in.Read(info, CObjectIStream::eNoFileHeader);
    if (typeInfo->IsCObject()) {
        CSerialObject* obj =
            reinterpret_cast<CSerialObject*>(info.GetObjectPtr());
        CConstRef<CSerialObject> ref(obj);
        m_Content.push_back(ref);
    }
    in.SetDiscardCurrObject();
}

const CTypeInfo* CSoapReadHook::x_FindType(const string& name)
{
    vector< const CTypeInfo* >::const_iterator it;
    for (it = m_Types.begin(); it != m_Types.end(); ++it) {
        if ((*it)->GetName() == name) {
            return (*it);
        }
    }
    return 0;
}


END_NCBI_SCOPE



/* --------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/09/22 21:00:04  gouriano
* Initial revision
*
*
* ===========================================================================
*/
