#ifndef GENERATE_HPP
#define GENERATE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <set>
#include <map>
#include "moduleset.hpp"

class CFileCode;

class CCodeGenerator : public CModuleContainer
{
public:
    typedef set<string> TTypeNames;
    typedef map<string, AutoPtr<CFileCode> > TOutputFiles;

    CCodeGenerator(void);
    ~CCodeGenerator(void);

    // setup interface
    void LoadConfig(const string& fileName);

    void IncludeTypes(const string& types);
    void ExcludeTypes(const string& types);
    void ExcludeRecursion(bool exclude = true)
        {
            m_ExcludeRecursion = exclude;
        }
    void IncludeAllMainTypes(void);

    void SetSourcesDir(const string& dir)
        {
            m_SourcesDir = dir;
        }
    void SetHeadersDir(const string& dir)
        {
            m_HeadersDir = dir;
        }
    void SetFileListFileName(const string& file)
        {
            m_FileListFileName = file;
        }

    CFileSet& GetMainModules(void)
        {
            return m_MainFiles;
        }
    CFileSet& GetImportModules(void)
        {
            return m_ImportFiles;
        }

    bool Check(void) const;

    void GenerateCode(void);

    bool Imported(const CDataType* type) const;

    // generation interface
    const CNcbiRegistry& GetConfig(void) const;
    const string& GetSourceFileName(void) const;
    CDataType* InternalResolve(const string& moduleName,
                               const string& typeName) const;

    CDataType* ExternalResolve(const string& module, const string& type,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& type,
                                  bool allowInternal = false) const;

    CDataType* ResolveMain(const string& fullName) const;

protected:

    static void GetTypes(TTypeNames& typeNames, const string& name);

    enum EContext {
        eOther,
        eRoot,
        eChoice,
        eReference
    };
    void CollectTypes(const CDataType* type, EContext context = eOther );
    bool AddType(const CDataType* type);

private:

    CNcbiRegistry m_Config;
    CFileSet m_MainFiles;
    CFileSet m_ImportFiles;
    TTypeNames m_GenerateTypes;
    bool m_ExcludeRecursion;
    string m_FileListFileName;
    string m_HeadersDir;
    string m_SourcesDir;
    string m_HeaderPrefix;

    TOutputFiles m_Files;
};

#endif
