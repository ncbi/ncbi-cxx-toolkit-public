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
*   Module container: base classe for module set
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/12/21 17:18:35  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

class CDataType;
class CDataTypeModule;

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

enum EHeadersDirNameSource {
    eFromNone,
    eFromSourceFileName,
    eFromModuleName
};

class CModuleContainer
{
public:
    CModuleContainer(void);
    virtual ~CModuleContainer(void);

    virtual const CNcbiRegistry& GetConfig(void) const;
    virtual const string& GetSourceFileName(void) const;
    virtual string GetHeadersPrefix(void) const;
    virtual EHeadersDirNameSource GetHeadersDirNameSource(void) const;
    virtual CDataType* InternalResolve(const string& moduleName,
                                       const string& typeName) const;

	void SetModuleContainer(const CModuleContainer* parent);
	const CModuleContainer& GetModuleContainer(void) const;
private:
    const CModuleContainer* m_Parent;
};

#endif
