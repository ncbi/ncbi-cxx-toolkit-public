#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <algorithm>
#include <set>
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

void GenerateCode(const CModuleSet& types, const set<string>& typeNames,
                  const string& fileName, const string& sourcesListName,
                  const string& headersDir, const string& sourcesDir)
{
    CNcbiRegistry def;
    if ( !fileName.empty() ) {
        // load descriptions from registry file
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

    string headersPath = MkDir(headersDir);
    string sourcesPath = MkDir(sourcesDir);
    string headersPrefix = MkDir(def.Get("-", "headers_prefix"));

    typedef list<const ASNType*> TClasses;
    typedef map<string, TClasses> TOutputFiles;
    TOutputFiles outputFiles;
    // sort types by output file
/*
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
*/
    for ( set<string>::const_iterator ti = typeNames.begin();
          ti != typeNames.end();
          ++ti ) {
        const ASNModule::TypeInfo* typeInfo = types.FindType(*ti);
        const ASNType* type = typeInfo->type;
        if ( !type ) {
            ERR_POST("Type " << *ti << " not found");
            continue;
        }
        if ( type->ClassName(def) == "-" )
            continue;
        
        outputFiles[type->FileName(def)].push_back(type);
    }
    
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

    if ( !sourcesListName.empty() ) {
        ofstream fileList(sourcesListName.c_str());
        
        fileList << "GENFILES =";
        for ( TOutputFiles::const_iterator filei = outputFiles.begin();
              filei != outputFiles.end();
              ++filei ) {
            fileList << ' ' << filei->first << "_Base";
        }
        fileList << endl;
    }
}
