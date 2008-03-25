#ifndef MODULECONTAINER_HPP
#define MODULECONTAINER_HPP

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
*   Module container: base classes for module set
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CNcbiRegistry;
class CDataType;
class CDataTypeModule;
class CNamespace;

enum EFileNamePrefixSource {
    eFileName_FromNone = 0,
    eFileName_FromSourceFileName = 1,
    eFileName_FromModuleName = 2,
    eFileName_UseAllPrefixes = 4
};

class CModuleContainer
{
public:
    CModuleContainer(void);
    virtual ~CModuleContainer(void);

    virtual const CNcbiRegistry& GetConfig(void) const;
    virtual const string& GetSourceFileName(void) const;
    virtual string GetFileNamePrefix(void) const;
    virtual EFileNamePrefixSource GetFileNamePrefixSource(void) const;
    bool MakeFileNamePrefixFromSourceFileName(void) const
        {
            return (GetFileNamePrefixSource() & eFileName_FromSourceFileName)
                != 0;
        }
    bool MakeFileNamePrefixFromModuleName(void) const
        {
            return (GetFileNamePrefixSource() & eFileName_FromModuleName)
                != 0;
        }
    bool UseAllFileNamePrefixes(void) const
        {
            return (GetFileNamePrefixSource() & eFileName_UseAllPrefixes)
                != 0;
        }
    virtual CDataType* InternalResolve(const string& moduleName,
                                       const string& typeName) const;

    virtual const CNamespace& GetNamespace(void) const;
    virtual string GetNamespaceRef(const CNamespace& ns) const;

	void SetModuleContainer(const CModuleContainer* parent);
	const CModuleContainer& GetModuleContainer(void) const;
private:
    const CModuleContainer* m_Parent;

    CModuleContainer(const CModuleContainer&);
    CModuleContainer& operator=(const CModuleContainer&);
};

END_NCBI_SCOPE

#endif
