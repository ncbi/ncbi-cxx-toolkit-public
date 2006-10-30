#ifndef MODULESET_HPP
#define MODULESET_HPP

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
*   Module set: equivalent of ASN.1 source file
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/mcontainer.hpp>
#include <serial/datatool/comments.hpp>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CDataType;
class CDataTypeModule;
class CFileModules;
class CFileSet;

class CFileModules : public CModuleContainer
{
public:
    typedef list< AutoPtr<CDataTypeModule> > TModules;
    typedef map<string, CDataTypeModule*> TModulesByName;

    CFileModules(const string& fileName);

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintASN(CNcbiOstream& out) const;
    void PrintSpecDump(CNcbiOstream& out) const;
    void PrintXMLSchema(CNcbiOstream& out) const;

    void GetRefInfo(list<string>& info) const;
    void PrintASNRefInfo(CNcbiOstream& out) const;
    void PrintXMLRefInfo(CNcbiOstream& out) const;

    void PrintDTD(CNcbiOstream& out) const;
    void PrintDTDModular(void) const;

    void PrintXMLSchemaModular(void) const;
    void BeginXMLSchema(CNcbiOstream& out) const;
    void EndXMLSchema(CNcbiOstream& out) const;

    const string& GetSourceFileName(void) const;
    string GetFileNamePrefix(void) const;

    void AddModule(const AutoPtr<CDataTypeModule>& module);

    const TModules& GetModules(void) const
        {
            return m_Modules;
        }

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;

    CComments& LastComments(void)
        {
            return m_LastComments;
        }

private:
    TModules m_Modules;
    TModulesByName m_ModulesByName;
    string m_SourceFileName;
    CComments m_LastComments;
    mutable string m_PrefixFromSourceFileName;

    friend class CFileSet;
};

class CFileSet : public CModuleContainer
{
public:
    typedef list< AutoPtr< CFileModules > > TModuleSets;

    void AddFile(const AutoPtr<CFileModules>& moduleSet);

    const TModuleSets& GetModuleSets(void) const
        {
            return m_ModuleSets;
        }
    TModuleSets& GetModuleSets(void)
        {
            return m_ModuleSets;
        }

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintASN(CNcbiOstream& out) const;
    void PrintSpecDump(CNcbiOstream& out) const;
    void PrintXMLSchema(CNcbiOstream& out) const;
    void PrintDTD(CNcbiOstream& out) const;

    void PrintDTDModular(void) const;
    void PrintXMLSchemaModular(void) const;

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;

private:
    TModuleSets m_ModuleSets;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2006/10/30 18:15:14  gouriano
* Added writing data specification in internal format
*
* Revision 1.14  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.13  2006/06/19 17:33:33  gouriano
* Redesigned generation of XML schema
*
* Revision 1.12  2006/06/01 18:42:27  gouriano
* Mark source spec info by a special tag
*
* Revision 1.11  2006/05/25 17:29:05  gouriano
* Added reference to the source when converting specification
*
* Revision 1.10  2005/06/06 17:41:07  gouriano
* Added generation of modular XML schema
*
* Revision 1.9  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.8  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.7  2000/11/29 17:42:30  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.6  2000/11/14 21:41:14  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/11/08 17:02:39  vasilche
* Added generation of modular DTD files.
*
* Revision 1.4  2000/08/25 15:58:47  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.3  2000/05/24 20:08:31  vasilche
* Implemented DTD generation.
*
* Revision 1.2  2000/04/07 19:26:10  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.12  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.11  1999/12/21 17:18:37  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.10  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.9  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.8  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
