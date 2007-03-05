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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *   Simple command-line app to split an object given a set of read hooks
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/iterator.hpp>
#include <serial/objectiter.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/gb_release_file.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CObjExtractApp::


class CObjExtractApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CObjExtractApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Object splitter/decontainerizer");

    arg_desc->AddDefaultKey("i", "InputFile",
                            "File to split",
                            CArgDescriptions::eInputFile,
                            "-");

    arg_desc->AddDefaultKey("ifmt", "InputFormat",
                            "Format of input file",
                            CArgDescriptions::eString,
                            "asn");
    arg_desc->SetConstraint("ifmt",
                            &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    arg_desc->AddDefaultKey("itype", "InputType",
                            "Type for input file",
                            CArgDescriptions::eString,
                            "Seq-entry");
    arg_desc->SetConstraint("itype",
                            &(*new CArgAllow_Strings,
                              "Seq-entry", "Bioseq", "Bioseq-set",
                              "Seq-annot", "Seq-align-set"));

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output for decontainered/found objects",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddDefaultKey("ofmt", "OutputFormat",
                            "Format of onput file",
                            CArgDescriptions::eString,
                            "asn");
    arg_desc->SetConstraint("ofmt",
                            &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    arg_desc->AddDefaultKey("otype", "OutputType",
                            "Type for onput file",
                            CArgDescriptions::eString,
                            "Seq-entry");
    arg_desc->SetConstraint("otype",
                            &(*new CArgAllow_Strings,
                              "Seq-entry", "Bioseq", "Bioseq-set",
                              "Seq-annot", "Seq-align-set"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


///
/// a simple read hook - when it finds its object, it reads it in and dumps it
/// to the stream
///
class CReadHookWriter : public CReadObjectHook
{
public:
    CReadHookWriter(const CTypeInfo* info,
                    CObjectOStream& ostr)
        : m_TypeInfo(info),
          m_Ostr(ostr)
    {
    }

    void ReadObject(CObjectIStream& istr,
                    const CObjectInfo& object)
    {
        DefaultRead(istr, object);
        CRef<CSerialObject> obj
            (static_cast<CSerialObject*>(object.GetObjectPtr()));
        m_Ostr.Write(obj, obj->GetThisTypeInfo());
        istr.SetDiscardCurrObject();
    }

private:
    const CTypeInfo* m_TypeInfo;
    CObjectOStream& m_Ostr;
};


///
/// handler for GenBank release files
///
class CGbEntryHandler : public CGBReleaseFile::ISeqEntryHandler
{
public:
    CGbEntryHandler(CObjectOStream& ostr)
        : m_Ostr(ostr)
        {
        }

    bool HandleSeqEntry(CRef<CSeq_entry>& entry)
    {
        m_Ostr.Write(entry, entry->GetThisTypeInfo());
        return true;
    }

private:
    CObjectOStream& m_Ostr;
};



int CObjExtractApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    ///
    /// establish our types
    ///
    CSeq_entry::GetTypeInfo();
    CBioseq::GetTypeInfo();
    CBioseq_set::GetTypeInfo();
    CSeq_annot::GetTypeInfo();
    CSeq_align_set::GetTypeInfo();

    ///
    /// get our args
    ///
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    ESerialDataFormat ifmt = eSerial_AsnText;
    {{
         string str = args["ifmt"].AsString();
         if (str == "asn") {
             ifmt = eSerial_AsnText;
         } else if (str == "asnb") {
             ifmt = eSerial_AsnBinary;
         } else if (str == "xml") {
             ifmt = eSerial_Xml;
         }
     }}
    const CTypeInfo* itype = CSeq_entry::GetTypeInfo();
    {{
         string str = args["itype"].AsString();
         const CTypeInfo* info = CClassTypeInfoBase::GetClassInfoByName(str);
         if (info) {
             itype = info;
         }
     }}

    ESerialDataFormat ofmt = eSerial_AsnText;
    {{
         string str = args["ofmt"].AsString();
         if (str == "asn") {
             ofmt = eSerial_AsnText;
         } else if (str == "asnb") {
             ofmt = eSerial_AsnBinary;
         } else if (str == "xml") {
             ofmt = eSerial_Xml;
         }
     }}
    const CTypeInfo* otype = CSeq_entry::GetTypeInfo();
    {{
         string str = args["otype"].AsString();
         const CTypeInfo* info = CClassTypeInfoBase::GetClassInfoByName(str);
         if (info) {
             otype = info;
         }
     }}

    ///
    /// now, process!
    ///
    auto_ptr<CObjectIStream> obj_istr(CObjectIStream::Open(ifmt, istr));
    auto_ptr<CObjectOStream> obj_ostr(CObjectOStream::Open(ofmt, ostr));


    LOG_POST(Error
             << "extracting " << otype->GetName()
             << " from "      << itype->GetName());
    if (itype == CBioseq_set::GetTypeInfo()  &&
        otype == CSeq_entry::GetTypeInfo()) {
        /// process as a GenBank Release File
        CGBReleaseFile rf(*obj_istr.release());
        rf.RegisterHandler(new CGbEntryHandler(*obj_ostr));
        rf.Read();
    } else {
        const_cast<CTypeInfo*>(otype)
            ->SetGlobalReadHook(new CReadHookWriter(otype, *obj_ostr));

        CRef<CSerialObject> obj(static_cast<CSerialObject*>(itype->Create()));
        obj_istr->Read(obj, obj->GetThisTypeInfo());
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CObjExtractApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CObjExtractApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
