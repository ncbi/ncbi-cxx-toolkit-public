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
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <list>
#include <map>

USING_NCBI_SCOPE;

class CDataType;
class CModuleContainer;

class CDataTypeModule {
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

    const string& GetVar(const string& section, const string& value) const;
    const string& GetSourceFileName(void) const;
    string GetHeadersPrefix(void) const;
    
    void AddDefinition(const string& name, const AutoPtr<CDataType>& type);
    void AddExports(const TExports& exports);
    void AddImports(const TImports& imports);
    void AddImports(const string& module, const list<string>& types);

    virtual void PrintASN(CNcbiOstream& out) const;

    bool Check();
    bool CheckNames();

    const string& GetName(void) const
        {
            return m_Name;
        }
    const CModuleContainer& GetModuleContainer(void) const
        {
            _ASSERT(m_ModuleContainer != 0);
            return *m_ModuleContainer;
        }
    void SetModuleContainer(const CModuleContainer* container);
    const TDefinitions& GetDefinitions(void) const
        {
            return m_Definitions;
        }

    // return type visible from inside, or throw CTypeNotFound if none
    CDataType* InternalResolve(const string& name) const;
    // return type visible from outside, or throw CTypeNotFound if none
    CDataType* ExternalResolve(const string& name,
                               bool allowInternal = false) const;

private:
    bool m_Errors;
    string m_Name;

    const CModuleContainer* m_ModuleContainer;

    TExports m_Exports;
    TImports m_Imports;
    TDefinitions m_Definitions;

    typedef map<string, CDataType*> TTypesByName;
    typedef map<string, string> TImportsByName;

    TTypesByName m_LocalTypes;
    TTypesByName m_ExportedTypes;
    TImportsByName m_ImportedTypes;
};

#endif
