#ifndef MODULE_HPP
#define MODULE_HPP

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
*   Type definitions module: equivalent of ASN.1 module
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <list>
#include <map>
#include <set>
#include <serial/datatool/mcontainer.hpp>
#include <serial/datatool/comments.hpp>

BEGIN_NCBI_SCOPE

class CDataType;

class CDataTypeModule : public CModuleContainer {
public:
    CDataTypeModule(const string& name);
    virtual ~CDataTypeModule();

    class Import {
    public:
        string moduleName;
        list<string> types;
    };
    typedef list< AutoPtr<Import> > TImports;
    typedef list< string > TExports;
    typedef list< pair< string, AutoPtr<CDataType> > > TDefinitions;

    bool Errors(void) const
        {
            return m_Errors;
        }

    const string GetVar(const string& section, const string& value) const;
    string GetFileNamePrefix(void) const;
    
    void AddDefinition(const string& name, const AutoPtr<CDataType>& type);
    void AddExports(const TExports& exports);
    void AddImports(const TImports& imports);
    void AddImports(const string& module, const list<string>& types);

    virtual void PrintASN(CNcbiOstream& out) const;
    virtual void PrintSpecDump(CNcbiOstream& out) const;
    virtual void PrintXMLSchema(CNcbiOstream& out) const;
    virtual void PrintDTD(CNcbiOstream& out) const;

    void PrintDTDModular(CNcbiOstream& out) const;
    void PrintXMLSchemaModular(CNcbiOstream& out) const;
    string GetDTDPublicName(void) const;
    string GetDTDFileNameBase(void) const;

    bool Check();
    bool CheckNames();

    const string& GetName(void) const
        {
            return m_Name;
        }
    const TDefinitions& GetDefinitions(void) const
        {
            return m_Definitions;
        }

    // return type visible from inside, or throw CTypeNotFound if none
    CDataType* Resolve(const string& name) const;
    // return type visible from outside, or throw CTypeNotFound if none
    CDataType* ExternalResolve(const string& name,
                               bool allowInternal = false) const;

    CComments& Comments(void)
        {
            return m_Comments;
        }
    CComments& LastComments(void)
        {
            return m_LastComments;
        }
    const TImports& GetImports(void) const
        {
            return m_Imports;
        }
    bool AddImportRef(const string& imp);
    
    static void SetModuleFileSuffix(const string& suffix)
    {
        s_ModuleFileSuffix = suffix;
    }
    static string GetModuleFileSuffix(void)
    {
        return s_ModuleFileSuffix;
    }
    static string ToAsnName(const string& name);
    static string ToAsnId(const string& name);

private:
    bool m_Errors;
    string m_Name;
    CComments m_Comments;
    CComments m_LastComments;
    mutable string m_PrefixFromName;

    TExports m_Exports;
    TImports m_Imports;
    TDefinitions m_Definitions;

    typedef map<string, CDataType*> TTypesByName;
    typedef map<string, string> TImportsByName;

    TTypesByName m_LocalTypes;
    TTypesByName m_ExportedTypes;
    TImportsByName m_ImportedTypes;
    set<string> m_ImportRef;
    static string s_ModuleFileSuffix;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2006/10/30 18:15:14  gouriano
* Added writing data specification in internal format
*
* Revision 1.16  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.15  2006/06/19 17:33:33  gouriano
* Redesigned generation of XML schema
*
* Revision 1.14  2006/05/16 14:30:29  gouriano
* Corrected generation of ASN spec - to make sure it is valid
*
* Revision 1.13  2006/04/13 12:59:08  gouriano
* Added optional file name suffix to modular DTD or schema
*
* Revision 1.12  2005/06/29 15:10:16  gouriano
* Resolve all module dependencies when generating modular DTD or schema
*
* Revision 1.11  2005/06/06 17:41:07  gouriano
* Added generation of modular XML schema
*
* Revision 1.10  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.9  2003/04/29 18:29:34  gouriano
* object data member initialization verification
*
* Revision 1.8  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.7  2000/11/29 17:42:30  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.6  2000/11/14 21:41:13  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/11/08 17:02:39  vasilche
* Added generation of modular DTD files.
*
* Revision 1.4  2000/08/25 15:58:46  vasilche
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
* Revision 1.1  2000/02/01 21:46:20  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.15  1999/12/29 16:01:51  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.14  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.13  1999/12/21 17:18:36  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.12  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.11  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.10  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
