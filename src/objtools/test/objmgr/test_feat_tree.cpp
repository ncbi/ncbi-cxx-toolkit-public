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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Tests for CFeatTree functions.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/stream_utils.hpp>
#include <util/format_guess.hpp>
#include <serial/objistr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(feature);


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//


class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("i", "InFile",
                     "file with features to process",
                     CArgDescriptions::eInputFile,
                     CArgDescriptions::fBinary);
    arg_desc->AddOptionalKey("format", "InFormat",
                             "format of input file",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("format",
                            &(*new CArgAllow_Strings,
                              "asn", "bin", "xml"));
    arg_desc->AddOptionalKey("type", "InType",
                             "type of object in the input file",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("type",
                            &(*new CArgAllow_Strings,
                              "Seq-entry", "Seq-annot"));
    arg_desc->AddOptionalKey("o", "OutFile",
                             "file with CFeatTree results",
                             CArgDescriptions::eOutputFile);

    arg_desc->AddFlag("timing", "Print time spent");
    arg_desc->AddFlag("verbose", "Print detailed feature info");

    // Program description
    string prog_description = "Test program for CFeatTree functionality\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


CSeq_id_Handle s_Normalize(const CSeq_id_Handle& id, CScope& scope)
{
    CSeq_id_Handle ret = scope.GetAccVer(id);
    return ret? ret: id;
}


typedef pair<string, CMappedFeat> TFeatureKey;
typedef set<TFeatureKey> TOrderedFeatures;
typedef map<CMappedFeat, TOrderedFeatures> TOrderedTree;
typedef map<TFeatureKey, size_t> TFeatureIndex;

TFeatureKey s_GetFeatureKey(const CMappedFeat& child)
{
    CNcbiOstrstream str;
    CRange<TSeqPos> range;
    try {
        range = child.GetLocation().GetTotalRange();
    }
    catch ( CException& ) {
    }
    str << setw(10) << range.GetFrom()
        << setw(10) << range.GetTo()
        << " " << MSerial_AsnText
        << child.GetMappedFeature();
    string s = CNcbiOstrstreamToString(str);
    return TFeatureKey(s, child);
}

void s_PrintTree(CNcbiOstream& out,
                 const string& p1, const string& p2,
                 TOrderedTree& tree, TFeatureKey key,
                 TFeatureIndex& index)
{
    const CMappedFeat& feat = key.second;
    const TOrderedFeatures& cc = tree[feat];
    out << p1 << "-F[" << index[key] << "]:"
        << CSeqFeatData::SelectionName(feat.GetFeatType())
        << "(subt " << feat.GetFeatSubtype() << ")";
    if ( feat.GetFeatType() == CSeqFeatData::e_Gene ) {
        const CGene_ref& gene = feat.GetOriginalFeature().GetData().GetGene();
        if ( gene.IsSetLocus() ) {
            out << " " << gene.GetLocus();
        }
        if ( gene.IsSetLocus_tag() ) {
            out << " tag=" << gene.GetLocus_tag();
        }
    }
    if ( feat.IsSetProduct() ) {
        out << " -> ";
        CConstRef<CSeq_id> id;
        try {
            id = feat.GetProduct().GetId();
        }
        catch ( CException& ) {
            out << "*bad loc*";
        }
        if ( id ) {
            out << s_Normalize(CSeq_id_Handle::GetHandle(*id),
                               feat.GetScope());
        }
    }
    out << " ";
    try {
        out << feat.GetLocation().GetTotalRange();
    }
    catch ( CException& ) {
        out << "*bad loc*";
    }
    out << NcbiEndl;
    ITERATE ( TOrderedFeatures, it, cc ) {
        TOrderedFeatures::const_iterator it2 = it;
        if ( ++it2 != cc.end() ) {
            s_PrintTree(out, p2+" +", p2+" |", tree, *it, index);
        }
        else {
            s_PrintTree(out, p2+" +", p2+"  ", tree, *it, index);
        }
    }
}


static
ESerialDataFormat s_GetFormat(const string& arg)
{
    ESerialDataFormat format = eSerial_None;
    if ( arg == "asn" ) {
        format = eSerial_AsnText;
    }
    else if ( arg == "bin" ) {
        format = eSerial_AsnBinary;
    }
    else if ( arg == "xml" ) {
        format = eSerial_Xml;
    }
    else {
        ERR_POST(Fatal<<"Unknown format name: "<<arg);
    }
    return format;
}


static
ESerialDataFormat s_GuessFormat(CNcbiIstream& in)
{
    ESerialDataFormat format = eSerial_None;
    switch ( CFormatGuess::Format(in) ) {
    case CFormatGuess::eTextASN:
        format = eSerial_AsnText;
        break;
    case CFormatGuess::eBinaryASN:
        format = eSerial_AsnBinary;
        break;
    case CFormatGuess::eXml:
        format = eSerial_Xml;
        break;
    default:
        ERR_POST(Fatal<<"Cannot determine input file format");
    }
    return format;
}


static
string s_GuessType(CNcbiIstream& in, ESerialDataFormat format)
{
    char buf[1024];
    in.read(buf, sizeof(buf));
    size_t size = in.gcount();
    if ( !size ) {
        return "";
    }
    in.clear();
    CStreamUtils::Pushback(in, buf, size);

    CNcbiIstrstream test_str(buf, size);
    auto_ptr<CObjectIStream> test_in(CObjectIStream::Open(format, test_str));
    if ( format == eSerial_AsnText || format == eSerial_Xml ) {
        return test_in->ReadFileHeader();
    }
    else {
        static const CTypeInfo* const types[] = {
            CType<CSeq_annot>().GetTypeInfo(),
            CType<CSeq_entry>().GetTypeInfo()
        };
        for ( size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i ) {
            const CTypeInfo* type = types[i];
            try {
                test_in->Skip(type);
                return type->GetName();
            }
            catch (CEofException) {
                // Not enough data in the test stream, but the blob type
                // is correct.
                return type->GetName();
            }
            catch (CSerialException ex) {
                if (ex.GetErrCode() == CSerialException::eEOF) {
                    // Same as CEofException
                    return type->GetName();
                }
                test_in->SetStreamPos(0); // rewind
            }
        }
    }
    return "";
}


static
void s_StartTiming(CStopWatch& sw, bool timing)
{
    if ( timing ) {
        sw.Start();
    }
}


static
void s_StopTiming(CStopWatch& sw, bool timing, const char* msg)
{
    if ( timing ) {
        NcbiCerr << msg<<" in " << sw.Elapsed() << "s" << NcbiEndl;
    }
}


int CTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    const bool timing = args["timing"];
    CStopWatch sw;
    
    CSeq_entry_Handle added_entry;
    CSeq_annot_Handle added_annot;

    s_StartTiming(sw, timing);

    CNcbiIstream& in = args["i"].AsInputFile();
    CNcbiOstream& out = args["o"]? args["o"].AsOutputFile(): NcbiCout;

    ESerialDataFormat format = args["format"]?
        s_GetFormat(args["format"].AsString()):
        s_GuessFormat(in);

    string type =
        args["type"]? args["type"].AsString(): s_GuessType(in, format);

    switch ( format ) {
    case eSerial_AsnText:
        in >> MSerial_AsnText;
        break;
    case eSerial_AsnBinary:
        in >> MSerial_AsnBinary;
        break;
    case eSerial_Xml:
        in >> MSerial_Xml;
        break;
    default:
        break;
    }
    if ( type == "Seq-entry" ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        in >> *entry;
        added_entry = scope.AddTopLevelSeqEntry(*entry);
    }
    else if ( type == "Seq-annot" ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        in >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }
    else {
        ERR_POST(Fatal<<"Unknown input object type");
    }

    s_StopTiming(sw, timing, "Loaded data");

    CFeatTree ft;
    //ft.SetFeatIdMode(feat_id_mode);
    //ft.SetSNPStrandMode(snp_strand_mode);

    s_StartTiming(sw, timing);
    if ( added_entry ) {
        ft.AddFeatures(CFeat_CI(added_entry));
    }
    if ( added_annot ) {
        ft.AddFeatures(CFeat_CI(added_annot));
    }
    s_StopTiming(sw, timing, "Added features");

    s_StartTiming(sw, timing);
    out << " Root features: " << ft.GetRootFeatures().size() << NcbiEndl;
    s_StopTiming(sw, timing, "Made tree");
    
    TOrderedTree tree;
    TOrderedFeatures all;
    TOrderedTree by_gene;
    list<CMappedFeat> q;
    q.push_back(CMappedFeat());
    ITERATE ( list<CMappedFeat>, pit, q ) {
        CMappedFeat parent = *pit;
        vector<CMappedFeat> cc = ft.GetChildren(parent);
        TOrderedFeatures& dst = tree[parent];
        ITERATE ( vector<CMappedFeat>, cit, cc ) {
            CMappedFeat child = *cit;
            TFeatureKey key = s_GetFeatureKey(child);
            dst.insert(key);
            all.insert(key);
            q.push_back(child);
            CMappedFeat gp = ft.GetParent(child, CSeqFeatData::eSubtype_gene);
            CMappedFeat g = ft.GetBestGene(child, ft.eBestGene_OverlappedOnly);
            if ( g && g != gp ) {
                if ( !by_gene.count(g) ) {
                    by_gene[CMappedFeat()].insert(s_GetFeatureKey(g));
                }
                by_gene[g].insert(key);
            }
        }
    }
    const bool verbose = args["verbose"];
    size_t cnt = 0;
    TFeatureIndex index;
    ITERATE ( TOrderedFeatures, fit, all ) {
        index[*fit] = cnt;
        if ( verbose ) {
            out << "Feature "<<cnt<<": " << fit->first;
        }
        ++cnt;
    }
    if ( verbose ) {
        out << "Tree:" << NcbiEndl;
        {
            out << "Root features: ";
            const TOrderedFeatures& cc = tree[CMappedFeat()];
            ITERATE ( TOrderedFeatures, cit, cc ) {
                out << " " << index[*cit];
            }
            out << NcbiEndl;
        }
        ITERATE ( TOrderedFeatures, fit, all ) {
            out << "Children of "<<index[*fit] << ": ";
            const TOrderedFeatures& cc = tree[fit->second];
            ITERATE ( TOrderedFeatures, cit, cc ) {
                out << " " << index[*cit];
            }
            out << NcbiEndl;
        }
        out << NcbiEndl;
    }
    {
        string prefix;
        out << "= Tree =" << NcbiEndl;
        const TOrderedFeatures& cc = tree[CMappedFeat()];
        ITERATE ( TOrderedFeatures, cit, cc ) {
            s_PrintTree(out, "", "", tree, *cit, index);
        }
        out << "= end tree =" << NcbiEndl;
    }
    if ( !by_gene.empty() ) {
        string prefix;
        out << "= By gene =" << NcbiEndl;
        const TOrderedFeatures& cc = by_gene[CMappedFeat()];
        ITERATE ( TOrderedFeatures, cit, cc ) {
            s_PrintTree(out, "", "", by_gene, *cit, index);
        }
        out << "= end by gene =" << NcbiEndl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}
