#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <moduleset.hpp>
#include <module.hpp>
#include <type.hpp>
#include <fstream>

USING_NCBI_SCOPE;

inline
string replace(string s, char from, char to)
{
    replace(s.begin(), s.end(), from, to);
    return s;
}

inline
string Identifier(const string& typeName)
{
    return replace(typeName, '-', '_');
}

inline
string ClassName(const CNcbiRegistry& reg, const string& typeName)
{
    const string& className = reg.Get(typeName, "_class");
    if ( className.empty() )
        return Identifier(typeName);
    else
        return className;
}

inline
string FileName(const CNcbiRegistry& reg, const string& typeName)
{
    const string& fileName = reg.Get(typeName, "_file");
    if ( fileName.empty() )
        return Identifier(typeName);
    else
        return fileName;
}

void GenerateCode(const CModuleSet& types, const string& fileName)
{
    CNcbiRegistry def;
    { // load descriptions from registry file
        ifstream fileIn;
        CNcbiIstream* in;
        if ( fileName == "stdin" || fileName == "-" ) {
            in = &NcbiCin;
        }
        else {
            fileIn.open(fileName.c_str());
            if ( !fileIn )
                ERR_POST(Fatal << "cannot open file " << fileName);
            in = &fileIn;
        }
        def.Read(*in);
    }

    string headersPath = def.Get("-", "headers_path");
    if ( !headersPath.empty() )
        headersPath += '/';
    string sourcesPath = def.Get("-", "sources_path");
    if ( !sourcesPath.empty() )
        sourcesPath += '/';
    string includeAdd = def.Get("-", "include_add");
    if ( !includeAdd.empty() )
        includeAdd += '/';
    string ns = def.Get("-", "namespace");

    typedef list<const ASNType*> TClasses;
    typedef map<string, TClasses> TOutputFiles;
    TOutputFiles outputFiles;
    // sort types by output file
    for ( CModuleSet::TModules::const_iterator modp = types.modules.begin();
          modp != types.modules.end();
          ++modp ) {
        const ASNModule& module = **modp;
        for ( ASNModule::TDefinitions::const_iterator defp =
                  module.definitions.begin();
              defp != module.definitions.end();
              ++defp ) {
            if ( ClassName(def, (*defp)->name) == "-" )
                continue;

            outputFiles[FileName(def, (*defp)->name)].push_back(defp->get());
        }
    }
    
    // generate output files
    for ( TOutputFiles::const_iterator filei = outputFiles.begin();
          filei != outputFiles.end();
          ++filei ) {
        string outFileName = filei->first;
        ofstream header((headersPath + outFileName + "_Base.hpp").c_str());
        ofstream code((sourcesPath + outFileName + "_Base.cpp").c_str());

        header <<
            "// This is generated file, don't modify" << endl <<
            "#ifndef " << filei->first << "_Base_HPP" << endl <<
            "#define " << filei->first << "_Base_HPP" << endl <<
            endl << "// standard includes" << endl <<
            "#include <corelib/ncbistd.hpp>" << endl;

        code << "// This is generated file, don't modify" << endl <<
            endl << "// standard includes" << endl <<
            "#include <serial/serial.hpp>" << endl;


        TClasses::const_iterator typei;

        set<string> includes;
        { // collect needed include files
            // collect used types
            set<string> usedTypes;
            for ( typei = filei->second.begin();
                  typei != filei->second.end();
                  ++typei ) {
                (*typei)->CollectUserTypes(usedTypes);
                string inc = def.Get((*typei)->name, "_include");
                if ( !inc.empty() )
                    includes.insert(inc);
            }

            // collect include file names
            for ( set<string>::const_iterator usedi = usedTypes.begin();
                  usedi != usedTypes.end();
                  ++usedi ) {
                includes.insert('<' + includeAdd +
                                FileName(def, *usedi) + ".hpp>");
            }
        }

        string mainInclude = '<' + includeAdd + outFileName + ".hpp>";
        code << endl << "// generated includes" << endl <<
            "#include " << mainInclude << endl;
        includes.erase(mainInclude);

        header << endl << "// generated includes" << endl;
        for ( set<string>::const_iterator includei = includes.begin();
              includei != includes.end();
              ++includei ) {
            header <<
                "#include " << *includei << endl;
        }

        if ( !ns.empty() ) {
            header << endl <<
                "namespace " << ns << " {" << endl;
            code << endl <<
                "namespace " << ns << " {" << endl;
        }
        header << endl << "// forward declarations" << endl;
        for ( typei = filei->second.begin();
              typei != filei->second.end();
              ++typei ) {
            const ASNType* type = *typei;
            string className = ClassName(def, (*typei)->name);

            header <<
                "class " << className << ';' << endl <<
                "class " << className << "_Base;" << endl;
        }

        header << endl << "// generated classes" << endl;
        code << endl << "// generated definitions" << endl;
        for ( typei = filei->second.begin();
              typei != filei->second.end();
              ++typei ) {
            string className = ClassName(def, (*typei)->name);

            header <<
                "class " << className << "_Base ";
            string baseType = def.Get((*typei)->name, "_parent");
            string baseClass = def.Get((*typei)->name, "_parent_class");
            if ( !baseType.empty() ) {
                header << " : public " << ClassName(def, baseType);
            }
            else if ( !baseClass.empty() ) {
                header << " : public " << baseClass;
            }
            header << endl <<
                '{' << endl <<
                "public:" << endl <<
                "    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void);" << endl <<
                "private:" << endl <<
                "    friend class " << className << ';' << endl << endl;

            code <<
                "BEGIN_CLASS_INFO3(\"" << (*typei)->name << "\", " <<
                className << "_Base" << ", " << className << ')' << endl <<
                '{' << endl;
            if ( !baseType.empty() ) {
                code <<
                    "    SET_PARENT_CLASS(" <<
                    ClassName(def, baseType) << ");" << endl;
            }
            
            (*typei)->GenerateCode(header, code, (*typei)->name, def, "");

            header << "};" << endl << endl;

            code <<
                '}' << endl <<
                "END_CLASS_INFO" << endl << endl;
        }

        if ( !ns.empty() ) {
            header << endl <<
                "};" << endl;
            code << endl <<
                "};" << endl;
        }
        header << endl <<
            "#endif // " << filei->first << "_Base_HPP" << endl;
    }
}
