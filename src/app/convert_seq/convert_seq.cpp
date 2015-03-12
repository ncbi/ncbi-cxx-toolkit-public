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
#include <util/xregexp/arg_regexp.hpp>
#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/gff_reader.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objtools/writers/agp_write.hpp>

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

    CRef<CSeq_entry> Read (const CArgs& args);
    void             Write(const CSeq_entry& entry, const CArgs& args);

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
                    "ID", "IDs", "asn", "asnb", "xml", "fasta", "gff", "tbl",
                    "agp"));
    arg_desc->AddOptionalKey
        ("infeat", "InputFile",
         "File from which to read an additional Sequin feature table",
         CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey
        ("inflags", "Flags",
         "Format-specific input flags, in C-style decimal, hex, or octal",
         CArgDescriptions::eString); // eInteger is too strict
    arg_desc->SetConstraint
        ("inflags",
         new CArgAllow_Regexp("^([1-9]\\d*|0[Xx][[:xdigit:]]*|0[0-7]*)$"));

    arg_desc->AddDefaultKey("out", "OutputFile", "File to write the object to",
                            CArgDescriptions::eOutputFile, "-");
    arg_desc->AddKey("outfmt", "Format", "Output format",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("outfmt", &(*new CArgAllow_Strings,
                     "asn", "asnb", "xml", "ddbj", "embl", "genbank", "fasta",
                     "gff", "gff3", "tbl", "gbseq/xml", "agp"));
    arg_desc->AddOptionalKey
        ("outflags", "Flags",
         "Format-specific output flags, in C-style decimal, hex, or octal"
         " (FASTA-only at present)",
         CArgDescriptions::eString); // eInteger is too strict
    arg_desc->SetConstraint
        ("outflags",
         new CArgAllow_Regexp("^([1-9]\\d*|0[Xx][[:xdigit:]]*|0[0-7]*)$"));
    arg_desc->AddFlag
        ("no-objmgr", "Bypass the object manager for FASTA output");

    arg_desc->AddFlag("gbload",
        "Use GenBank data loader");
    arg_desc->AddFlag("wgsload",
        "Use WGS data loader");

    SetupArgDescriptions(arg_desc.release());
}


int CConversionApp::Run(void)
{
    const CArgs& args = GetArgs();

    m_ObjMgr = CObjectManager::GetInstance();
    if ( args["wgsload"] ) {
        CWGSDataLoader::RegisterInObjectManager(*m_ObjMgr, CObjectManager::eDefault);
    }
	if ( args["gbload"] || !args["no-objmgr"] ) {
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
	}

    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddDefaults();

    CRef<CSeq_entry> entry = Read(args);
    if (args["infeat"]) {
        CFeature_table_reader::ReadSequinFeatureTables
            (args["infeat"].AsInputFile(), *entry);
    }
    if (!NStr::StartsWith(args["infmt"].AsString(), "ID")
        &&  GetSerialFormat(args["outfmt"].AsString()) == eSerial_None
        &&  (args["outfmt"].AsString() != "fasta"  ||  !args["no-objmgr"])) {
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
    TFormatElem("agp",       CFlatFileConfig::eFormat_AGP),
    TFormatElem("ddbj",      CFlatFileConfig::eFormat_DDBJ),
    TFormatElem("embl",      CFlatFileConfig::eFormat_EMBL),
    TFormatElem("gbseq/xml", CFlatFileConfig::eFormat_GBSeq),
    TFormatElem("genbank",   CFlatFileConfig::eFormat_GenBank),
    TFormatElem("gff",       CFlatFileConfig::eFormat_GFF),
    TFormatElem("gff3",      CFlatFileConfig::eFormat_GFF3),
    TFormatElem("tbl",       CFlatFileConfig::eFormat_FTable)
};
typedef CStaticArrayMap<const char*, TFFFormat, PNocase_CStr> TFormatMap;
DEFINE_STATIC_ARRAY_MAP(TFormatMap, sc_FormatMap, sc_FormatArray);

int CConversionApp::GetFlatFormat(const string& name)
{
    TFormatMap::const_iterator it = sc_FormatMap.find(name.c_str());
    if (it == sc_FormatMap.end()) {
        return -1;
    } else {
        return it->second;
    }
}


CRef<CSeq_entry> CConversionApp::Read(const CArgs& args)
{
    const string& infmt   = args["infmt"].AsString();
    const string& type    = args["type" ].AsString();
    int           inflags = 0;

    if (args["inflags"]) {
        inflags = NStr::StringToInt(args["inflags"].AsString(), 0, 0);
    } else if (infmt == "gff") { // set a non-trivial default
        inflags = (CGFFReader::fGBQuals | CGFFReader::fMergeExons
                   | CGFFReader::fSetProducts | CGFFReader::fCreateGeneFeats);
    }

    if (infmt == "ID") {
        CSeq_id        id(args["in"].AsString());
        CBioseq_Handle h = m_Scope->GetBioseqHandle(id);
        return CRef<CSeq_entry>
            (const_cast<CSeq_entry*>
             (h.GetTopLevelEntry().GetCompleteSeq_entry().GetPointer()));
    } else if (infmt == "IDs") {
        CNcbiIstream& in(args["in"].AsInputFile());
        string s;
        CRef<CSeq_entry> se(new CSeq_entry);
        while (in >> s) {
            CSeq_id id(s);
            CBioseq_Handle h = m_Scope->GetBioseqHandle(id);
            CConstRef<CSeq_entry> se2
                (h.GetTopLevelEntry().GetCompleteSeq_entry());
            se->SetSet().SetSeq_set().push_back
                (CRef<CSeq_entry>(const_cast<CSeq_entry*>(se2.GetPointer())));
        }
        return se;
    } else if (infmt == "fasta") {
        // return ReadFasta(args["in"].AsInputFile());
        CFastaReader fasta_reader(args["in"].AsString(), inflags);
        return fasta_reader.ReadSet();
    } else if (infmt == "gff") {
        return CGFFReader().Read(args["in"].AsInputFile(), inflags);
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
            (args["in"].AsInputFile(), inflags);
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
        if (inflags != 0) {
            for (CTypeIterator<CBioseq> it(*entry);  it;  ++it) {
                it->PackAsDeltaSeq(true);
            }
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
        CSeq_entry_Handle seh = m_Scope->GetSeq_entryHandle(entry);
        ff.Generate(seh, out);
        if (outfmt == "gff3") {
            out << "##FASTA" << endl;
            CFastaOstream fasta_out(out);
            fasta_out.Write(seh);
        }
    } else if (outfmt == "fasta") {
        CFastaOstream out(args["out"].AsOutputFile());
        if (args["outflags"]) {
            out.SetAllFlags
                (NStr::StringToInt(args["outflags"].AsString(), 0, 0));
        } else {
            out.SetFlag(CFastaOstream::fAssembleParts);
            out.SetFlag(CFastaOstream::fInstantiateGaps);
        }
        if (args["no-objmgr"]) {
            out.Write(entry, NULL /* location */, true /* no scope */);
        } else {
            out.Write(m_Scope->GetSeq_entryHandle(entry));
        }
    } else if (outfmt == "agp") {
        CNcbiOstream& out = args["out"].AsOutputFile();

		//m_topseh = m_Scope->AddTopLevelSeqEntry( *m_entry );
		unsigned long long num = 0;
		VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, entry) {
			const CBioseq& bioseq = *bit;
			if (bioseq.IsNa()) {
				++num;
				const CBioseq_Handle& bs = (*m_Scope).GetBioseqHandle (bioseq);
				string id; // = bs.GetSeqId()->GetSeqIdString(&version);
				bs.GetSeqId()->GetLabel(&id, CSeq_id::eContent, CSeq_id::fLabel_Version);
				if (id.empty())
				{
					id = "chr" + NStr::NumericToString(num);
				}
				AgpWrite( out, bs, id, vector<char>(), nullptr );
			}
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
