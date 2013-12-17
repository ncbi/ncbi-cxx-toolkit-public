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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   source qualifier generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/src_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
class CSrcChkApp : public CNcbiApplication
//  ============================================================================
{
public:
    void Init();
    int Run();

private:
    CNcbiOstream* xInitOutputStream(
        const CArgs&);
    bool xTryProcessIdFile(
        const CArgs&);
    bool xGetDesiredFields(
        const CArgs&,
        vector<string>&);
    CSrcWriter* xInitWriter(
        const CArgs&);

private:
    CRef<CObjectManager> m_pObjMngr;
    CRef<CScope> m_pScope;
    CRef<CSrcWriter> m_pWriter;
};

//  ----------------------------------------------------------------------------
void CSrcChkApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Extract Genbank source qualifiers",
        false);
    
    // input
    {{
        arg_desc->AddOptionalKey("i", "IDsFile", 
            "IDs file name", CArgDescriptions::eInputFile );
    }}

    // parameters
    {{
        arg_desc->AddOptionalKey("f", "FieldsList",
            "List of fields", CArgDescriptions::eString );
        arg_desc->AddOptionalKey("F", "FieldsFile",
            "File of fields", CArgDescriptions::eInputFile );
    }}
    // output
    {{ 
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile );
    }}
    {{
    //  misc
        arg_desc->AddDefaultKey("delim", "Delimiter",
            "Column value delimiter", CArgDescriptions::eString, "\t");
    }}
    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int CSrcChkApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

	CONNECT_Init(&GetConfig());
    m_pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_pObjMngr);
    m_pScope.Reset(new CScope(*m_pObjMngr));
    m_pScope->AddDefaults();

    m_pWriter.Reset(xInitWriter(args));
    if (xTryProcessIdFile(args)) {
        return 0;
    }
    return 1;
}

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xTryProcessIdFile(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["i"]) {
        return false;
    }
    CNcbiOstream* pOs = xInitOutputStream(args);

    const streamsize maxLineSize(100);
    char line[maxLineSize];

    CSrcWriter::FIELDS desiredFields;
    if (!xGetDesiredFields(args, desiredFields)) {
        return false;
    }

    CNcbiIstream& ifstr = args["i"].AsInputFile();
    vector<CBioseq_Handle> vecBsh;
    while (!ifstr.eof()) {
        ifstr.getline(line, maxLineSize);
        if (line[0] == 0  ||  line[0] == '#') {
            continue;
        }
        string id(line);
        NStr::TruncateSpacesInPlace(id);
        CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(id);
        CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
        if (!bsh) {
            return false;
        }
        vecBsh.push_back(bsh);
    }
    if (!m_pWriter->WriteBioseqHandles(vecBsh, desiredFields, *pOs)) {
        return false;
    }
    return true;
}    

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xGetDesiredFields(
    const CArgs& args,
    CSrcWriter::FIELDS& fields)
//  -----------------------------------------------------------------------------
{
    if (args["f"]) {
        string fieldString = args["f"].AsString();
        NStr::Tokenize(fieldString, ",", fields);
        return CSrcWriter::ValidateFields(fields);
    }
    if (args["F"]) {
        const streamsize maxLineSize(100);
        char line[maxLineSize];
        CNcbiIstream& ifstr = args["F"].AsInputFile();
        while (!ifstr.eof()) {
            ifstr.getline(line, maxLineSize);
            if (line[0] == 0  ||  line[0] == '#') {
                continue;
            }
            string field(line);
            NStr::TruncateSpacesInPlace(field);
            if (field.empty()) {
                continue;
            }
            fields.push_back(field);
        }
        return CSrcWriter::ValidateFields(fields);
    }
    fields.assign(
        CSrcWriter::sDefaultFields.begin(), CSrcWriter::sDefaultFields.end());
    return true;
}

//  -----------------------------------------------------------------------------
CNcbiOstream* CSrcChkApp::xInitOutputStream(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (!args["o"]) {
        return &cout;
    }
    try {
        return &args["o"].AsOutputFile();
    }
    catch(...) {
        NCBI_THROW(CObjWriterException, eArgErr, 
            "xInitOutputStream: Unable to create output file \"" +
            args["o"].AsString() +
            "\"");
    }
}

//  ----------------------------------------------------------------------------
CSrcWriter* CSrcChkApp::xInitWriter(
    const CArgs& args)
//  ----------------------------------------------------------------------------
{
    CSrcWriter* pWriter = new CSrcWriter(0);
    pWriter->SetDelimiter(args["delim"].AsString());
    return pWriter;
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
	return CSrcChkApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
