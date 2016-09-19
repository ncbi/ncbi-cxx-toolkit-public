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
* Author:  Justin Foley
*
* File Description:
*   Feature deconcatenor for .gff files
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE


class CGffDeconcatException : public CException
{
public:
    enum EErrCode {
        eInvalidOutputDir,
    };

    virtual const char* GetErrCodeString() const {
        switch (GetErrCode()) {
            case eInvalidOutputDir:
                return "eInvalidOutputDir";
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGffDeconcatException, CException);
};


class CGffDeconcatApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    using TIdmap = map<string, list<string>>;

private:
    void xProcessFile(CNcbiIstream& istr);
    void xReadFile(CNcbiIstream& istr, string& header, TIdmap& id_map);
    void xProcessLine(const string& line, TIdmap& id_map);
    void xWriteFile(CNcbiOfstream& ostr,
                    const string& header, 
                    const list<string>& body);
    void xSetExtension(const string& input_filename);

    string m_OutputDir;
    string m_Extension;
};


void CGffDeconcatApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());


    arg_desc->AddKey("i", "InputFile",
                     "GFF input filename", 
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("dir",
                             "OutputDirectory",
                             "Output Directory. Defaults to CWD",
                             CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());
}


void CGffDeconcatApp::xSetExtension(const string& input_filename)
{
    CDir input_file(input_filename);
    string trial_ext(input_file.GetExt());

    if (!NStr::IsBlank(trial_ext)) {
        try {
            NStr::StringToDouble(trial_ext); // Extension cannot be numeric
        }
        catch(...) {
            m_Extension = trial_ext;    
            return;
        }
    }

    m_Extension = ".gff";
}


int CGffDeconcatApp::Run(void)
{
    const CArgs& args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();

    xSetExtension(args["i"].AsString());

    if (args["dir"]) {
        auto dirname = args["dir"].AsString();

        if (!CDir::IsAbsolutePath(dirname)) {
            dirname = CDir::CreateAbsolutePath(dirname);
        }

        CDir output_dir(dirname);
        if (!output_dir.Exists()) {
            string err_msg = dirname 
                           + " does not exist";
            NCBI_THROW(CGffDeconcatException, 
                       eInvalidOutputDir,
                       err_msg);
        }

        m_OutputDir = dirname;
    }

    xProcessFile(istr);

    return 0;
}


void CGffDeconcatApp::xProcessFile(CNcbiIstream& istr) {

    TIdmap id_map;
    string header;
    xReadFile(istr, header, id_map);

    string output_dir = ".";
    if (!NStr::IsBlank(m_OutputDir)) {
        output_dir = m_OutputDir;
    }

    for (const auto& key_val : id_map) {
        string filename = output_dir + "/" 
                        + key_val.first 
                        + m_Extension;

        unique_ptr<CNcbiOfstream> ostr(new CNcbiOfstream(filename.c_str()));
        xWriteFile(*ostr, header, key_val.second);
    }
    return;
}


void CGffDeconcatApp::xProcessLine(const string& line, TIdmap& id_map) {
    vector<string> columns;
    NStr::Split(line, " \t", columns, NStr::eMergeDelims);
    if (columns.size() <= 1) {
        return;
    }    
    id_map[columns[0]].push_back(line);
}


void CGffDeconcatApp::xReadFile(CNcbiIstream& istr, string& header, TIdmap& id_map)
{
    id_map.clear();
    CStreamLineReader lr(istr);

    string line;
    while ( !lr.AtEOF() ) {
        line = *++lr;
        if (NStr::IsBlank(line) || 
            line[0] == '#') {
            if (line.size() > 2 &&
                line[1] == '#' && 
                header.empty() ) {
                header = line;
            }
            continue;
        }
        xProcessLine(line, id_map);
    }
    return;
}


void CGffDeconcatApp::xWriteFile(CNcbiOfstream& ostr, 
                                 const string& header, 
                                 const list<string>& body) 
{
    if (!NStr::IsBlank(header)) {
       ostr << header << "\n";
    }

    for (const string& line : body) {
        ostr << line << "\n";
    }

    return;
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CGffDeconcatApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
