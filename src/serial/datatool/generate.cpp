#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <fstream>
#include <algorithm>
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "code.hpp"

USING_NCBI_SCOPE;

inline
string MkDir(const string& s)
{
    SIZE_TYPE length = s.size();
    if ( length == 0 || s[length-1] == '/' )
        return s;
    else
        return s + '/';
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

    string headersPath = MkDir(def.Get("-", "headers_path"));
    string sourcesPath = MkDir(def.Get("-", "sources_path"));
    string headersPrefix = MkDir(def.Get("-", "headers_prefix"));
    string fileListName = def.Get("-", "file_list");

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
            const ASNType* type = defp->get();
            if ( type->ClassName(def) == "-" )
                continue;

            outputFiles[type->FileName(def)].push_back(type);
        }
    }
    
    //    auto_ptr<ofstream> files;
    //    ofstream files((sourcesPath + fileName + );
    // generate output files
    for ( TOutputFiles::const_iterator filei = outputFiles.begin();
          filei != outputFiles.end();
          ++filei ) {
        CFileCode code(def, filei->first, headersPrefix);
        for ( TClasses::const_iterator typei = filei->second.begin();
              typei != filei->second.end();
              ++typei ) {
            code.AddType(*typei);
        }
        code.GenerateHPP(headersPath);
        code.GenerateCPP(sourcesPath);
    }

    if ( !fileListName.empty() ) {
        ofstream fileList(fileListName.c_str());
        
        for ( TOutputFiles::const_iterator filei = outputFiles.begin();
              filei != outputFiles.end();
              ++filei ) {
            fileList << filei->first;
        }
    }
}
