#ifndef ANNOT_NAME__HPP
#define ANNOT_NAME__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotations selector structure.
*
*/


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CAnnotName
{
public:
    CAnnotName(void)
        : m_Named(false)
        {
        }
    CAnnotName(const string& name)
        : m_Named(true), m_Name(name)
        {
        }
    CAnnotName(const char* name)
        : m_Named(true), m_Name(name)
        {
        }

    bool IsNamed(void) const
        {
            return m_Named;
        }
    const string& GetName(void) const
        {
            _ASSERT(m_Named);
            return m_Name;
        }

    void SetUnnamed(void)
        {
            m_Named = false;
            m_Name.erase();
        }
    void SetNamed(const string& name)
        {
            m_Name = name;
            m_Named = true;
        }

    bool operator<(const CAnnotName& name) const
        {
            return name.m_Named && (!m_Named || name.m_Name > m_Name);
        }
    bool operator==(const CAnnotName& name) const
        {
            return name.m_Named == m_Named && name.m_Name == m_Name;
        }
    bool operator!=(const CAnnotName& name) const
        {
            return name.m_Named != m_Named || name.m_Name != m_Name;
        }

private:
    bool   m_Named;
    string m_Name;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_NAME__HPP
