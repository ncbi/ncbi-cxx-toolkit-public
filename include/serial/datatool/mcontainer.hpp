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
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.3  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.2  2000/04/07 19:26:09  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:20  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.4  1999/12/30 21:33:39  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.3  1999/12/29 16:01:50  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.2  1999/12/28 18:55:58  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.1  1999/12/21 17:18:35  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* ===========================================================================
*/
