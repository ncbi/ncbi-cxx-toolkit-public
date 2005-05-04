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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Program to convert biological sequences between the formats the
*   C++ Toolkit supports.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/static_map.hpp>
#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/gff_reader.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/agp_read.hpp>

// On Mac OS X 10.3, FixMath.h defines ff as a one-argument macro(!)
#ifdef ff
#  undef ff
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


class CConversionApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);

private:
    static int               GetFlatFormat  (const string& name);
    static ESerialDataFormat GetSerialFormat(const string& name);

    CConstRef<CSeq_entry> Read (const CArgs& args);
    void                  Write(const CSeq_entry& entry, const CArgs& args);

    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
};


void CConversionApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Convert biological sequences between formats",
                              false);

    arg_desc->AddDefaultKey("type", "AsnType", "Type of object to convert",
                            CArgDescriptions::eString, "Seq-entry");
    arg_desc->SetConstraint("type", &(*new CArgAllow_Strings,
                                      "Bioseq", "Bioseq-set", "Seq-entry"));

    arg_desc->AddDefaultKey("in", "InputFile", "File to read the object from",
                            CArgDescriptions::eInputFile, "-");
    arg_desc->AddKey("infmt", "Format", "Input format",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings,
                    "ID", "asn", "asnb", "xml", "fasta", "gff", "tbl", "agp"));

    arg_desc->AddDefaultKey("out", "OutputFile", "File to write the object to",
                            CArgDescriptions::eOutputFile, "-");
    arg_desc->AddKey("outfmt", "Format", "Output format",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("outfmt", &(*new CArgAllow_Strings,
                     "asn", "asnb", "xml", "ddbj", "embl", "genbank", "fasta",
                     "gff", "gff3", "tbl", "gbseq/xml"));

    SetupArgDescriptions(arg_desc.release());
}


int CConversionApp::Run(void)
{
    const CArgs& args = GetArgs();

    m_ObjMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddDefaults();

    CConstRef<CSeq_entry> entry = Read(args);
    if (args["infmt"].AsString() != "ID") {
        try {
            m_Scope->AddTopLevelSeqEntry(const_cast<CSeq_entry&>(*entry));
        } STD_CATCH_ALL("loading into OM")
    }
    Write(*entry, args);
    return 0;
}


ESerialDataFormat CConversionApp::GetSerialFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else {
        return eSerial_None;
    }
}


typedef CFlatFileConfig::TFormat TFFFormat;
typedef pair<const char*, TFFFormat> TFormatElem;
static const TFormatElem sc_FormatArray[] = {
    TFormatElem("ddbj",      CFlatFileConfig::eFormat_DDBJ),
    TFormatElem("embl",      CFlatFileConfig::eFormat_EMBL),
    TFormatElem("gbseq/xml", CFlatFileConfig::eFormat_GBSeq),
    TFormatElem("genbank",   CFlatFileConfig::eFormat_GenBank),
    TFormatElem("gff",       CFlatFileConfig::eFormat_GFF),
    TFormatElem("gff3",      CFlatFileConfig::eFormat_GFF3),
    TFormatElem("tbl",       CFlatFileConfig::eFormat_FTable)
};
typedef CStaticArrayMap<const char*, TFFFormat, PNocase_CStr> TFormatMap;
static const TFormatMap sc_FormatMap(sc_FormatArray, sizeof(sc_FormatArray));

int CConversionApp::GetFlatFormat(const string& name)
{
    TFormatMap::const_iterator it = sc_FormatMap.find(name.c_str());
    if (it == sc_FormatMap.end()) {
        return -1;
    } else {
        return it->second;
    }
}


CConstRef<CSeq_entry> CConversionApp::Read(const CArgs& args)
{
    const string& infmt = args["infmt"].AsString();
    const string& type  = args["type" ].AsString();

    if (infmt == "ID") {
        CSeq_id        id(args["in"].AsString());
        CBioseq_Handle h = m_Scope->GetBioseqHandle(id);
        return h.GetTopLevelEntry().GetCompleteSeq_entry();
    } else if (infmt == "fasta") {
        return ReadFasta(args["in"].AsInputFile());
    } else if (infmt == "gff") {
        return CGFFReader().Read(args["in"].AsInputFile(),
                                 CGFFReader::fGBQuals);
    } else if (infmt == "agp") {
        CRef<CBioseq_set> bss = AgpRead(args["in"].AsInputFile());
        if (bss->GetSeq_set().size() == 1) {
            return bss->GetSeq_set().front();
        } else {
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSet(*bss);
            return entry;
        }
    } else if (infmt == "tbl") {
        CRef<CSeq_annot> annot = CFeature_table_reader::ReadSequinFeatureTable
            (args["in"].AsInputFile());
        CRef<CSeq_entry> entry(new CSeq_entry);
        if (type == "Bioseq") {
            CBioseq& seq = entry->SetSeq();
            for (CTypeIterator<CSeq_id> it(*annot);  it;  ++it) {
                seq.SetId().push_back(CRef<CSeq_id>(&*it));
                BREAK(it);
            }
            seq.SetInst().SetRepr(CSeq_inst::eRepr_virtual);
            seq.SetInst().SetMol(CSeq_inst::eMol_not_set);
            seq.SetAnnot().push_back(annot);
        } else {
            entry->SetSet().SetAnnot().push_back(annot);
        }
        return entry;
    } else {
        CRef<CSeq_entry> entry(new CSeq_entry);
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(GetSerialFormat(infmt),
                                  args["in"].AsString(),
                                  eSerial_StdWhenDash));
        if (type == "Bioseq") {
            *in >> entry->SetSeq();
        } else if (type == "Bioseq-set") {
            *in >> entry->SetSet();
        } else {
            *in >> *entry;
        }
        return entry;
    }
}


void CConversionApp::Write(const CSeq_entry& entry, const CArgs& args)
{
    const string& outfmt = args["outfmt"].AsString();
    const string& type   = args["type"  ].AsString();
    int           ff_fmt = GetFlatFormat(outfmt);
    if (ff_fmt >= 0) {
        CNcbiOstream& out = args["out"].AsOutputFile();
        CFlatFileGenerator ff(static_cast<CFlatFileConfig::TFormat>(ff_fmt),
                              CFlatFileConfig::eMode_Entrez,
                              CFlatFileConfig::eStyle_Normal, 0,
                              CFlatFileConfig::fViewAll);
        ff.Generate(m_Scope->GetSeq_entryHandle(entry), out);
        if (outfmt == "gff3") {
            out << "##FASTA" << endl;
            CFastaOstream fasta_out(out);
            for (CTypeConstIterator<CBioseq> it(entry);  it;  ++it) {
                fasta_out.Write(m_Scope->GetBioseqHandle(*it));
            }
        }
    } else if (outfmt == "fasta") {
        CFastaOstream out(args["out"].AsOutputFile());
        for (CTypeConstIterator<CBioseq> it(entry);  it;  ++it) {
            out.Write(m_Scope->GetBioseqHandle(*it));
        }
    } else {
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(GetSerialFormat(outfmt),
                                  args["out"].AsString(),
                                  eSerial_StdWhenDash));
        if (type == "Bioseq") {
            if (entry.IsSet()) {
                ERR_POST(Warning
                         << "Possible truncation in conversion to Bioseq");
                CTypeConstIterator<CBioseq> it(entry);
                *out << *it;
            } else {
                *out << entry.GetSeq();
            }
        } else if (type == "Bioseq-set") {
            if (entry.IsSet()) {
                *out << entry.GetSet();
            } else {
                CBioseq_set bss;
                bss.SetSeq_set().push_back
                    (CRef<CSeq_entry>(const_cast<CSeq_entry*>(&entry)));
                *out << bss;
            }
        } else {
            *out << entry;
        }
    }
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CConversionApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.9  2005/05/04 19:05:00  ucko
* Take advantage of PNoase_CStr when comparing C strings.
*
* Revision 1.8  2004/11/01 19:33:08  grichenk
* Removed deprecated methods
*
* Revision 1.7  2004/07/21 15:51:24  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.6  2004/06/22 15:33:04  ucko
* Migrate to new flat-file generation library.
*
* Revision 1.5  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/05/03 18:01:34  ucko
* Kill unwanted definition of ff as a macro, if present (as on Mac OS 10.3)
*
* Revision 1.3  2004/02/27 20:07:10  jcherry
* Added agp as input format
*
* Revision 1.2  2004/01/05 17:59:32  vasilche
* Moved genbank loader and its readers sources to new location in objtools.
* Genbank is now in library libncbi_xloader_genbank.
* Id1 reader is now in library libncbi_xreader_id1.
* OBJMGR_LIBS macro updated correspondingly.
*
* Old headers temporarily will contain redirection to new location
* for compatibility:
* objmgr/gbloader.hpp > objtools/data_loaders/genbank/gbloader.hpp
* objmgr/reader_id1.hpp > objtools/data_loaders/genbank/readers/id1/reader_id1.hpp
*
* Revision 1.1  2003/12/03 20:58:40  ucko
* Add new universal sequence converter app.
*
*
* ===========================================================================
*/
