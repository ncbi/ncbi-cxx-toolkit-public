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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/writers/writer_message.hpp>
#include <objtools/writers/writer_listener.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>
#include <objtools/writers/bed_writer.hpp>
#include <objtools/writers/bedgraph_writer.hpp>
#include <objtools/writers/vcf_writer.hpp>
#include <objtools/writers/gvf_writer.hpp>
#include <objtools/writers/aln_writer.hpp>
#include <objtools/writers/psl_writer.hpp>
#include <objtools/readers/format_guess_ex.hpp>

#include <algo/sequence/gene_model.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const CDataLoadersUtil::TLoaders default_loaders = CDataLoadersUtil::fAsnCache | CDataLoadersUtil::fGenbank;

//#define CANCELER_CODE
#if defined(CANCELER_CODE)
//  ============================================================================
class TestCanceler: public ICanceled
//  ============================================================================
{
    static const unsigned int CALLS_UNTIL_CANCELLED = 25;
    bool IsCanceled() const {
        if (0 == ++mNumCalls % 100) {
            cerr << "Iterations until cancelled: "
                 << (CALLS_UNTIL_CANCELLED - mNumCalls) << "\n";
        }
        return (mNumCalls > CALLS_UNTIL_CANCELLED);
    };
    static unsigned int mNumCalls;
};
unsigned int TestCanceler::mNumCalls = 0;
#endif

//  ----------------------------------------------------------------------------
class CAnnotWriterLogger: public CWriterListener
//  ----------------------------------------------------------------------------
{
public:
    CAnnotWriterLogger() = default;

    bool PutMessage(
        const IObjtoolsMessage& message)
    {
        const auto* pWriterMessage = dynamic_cast<const CWriterMessage*>(&message);
        if (!pWriterMessage) {
            throw CWriterMessage("system: unrecoverable internal error", eDiag_Fatal);
        }
        if (pWriterMessage->GetSeverity() == eDiag_Fatal) {
            throw(*pWriterMessage);
        }
        DumpMessage(*pWriterMessage, cerr);
        return true;
    }

    static void DumpMessage(const CWriterMessage& message, ostream& ostr)
    {
        ostr << CNcbiDiag::SeverityName(message.GetSeverity()) << ":" << endl;
        ostr << message.GetText() << endl;
        ostr << endl;
    }

private:
};

//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    CAnnotWriterApp();
    void Init() override;
    int Run() override;

private:
    CNcbiOstream* xInitOutputStream(
        const CArgs& );
    CObjectIStream* xInitInputStream(
        const CArgs& args,
        CFileContentInfo& contentInfo);
    CWriterBase* xInitWriter(
        const CArgs&,
        CNcbiOstream* );

    bool xTryProcessInputId(
        const CArgs& );

    bool xTryProcessInputId(
        const string& id, bool skipHeaders);

    bool xTryProcessInputIdList(
        const CArgs& );

    bool xTryProcessInputFile(
        const CArgs& );

    bool xProcessInputObject(
        CSeq_entry&);

    bool xProcessInputObject(
        CSeq_entry& entry,
        bool skipHeaders);

    bool xProcessInputObject(
        CSeq_submit&);
    bool xProcessInputObject(
        CSeq_annot&);
    bool xProcessInputObject(
        CBioseq&);
    bool xProcessInputObject(
        CBioseq& bioseq,
        bool skipHeaders);
    bool xProcessInputObject(
        CBioseq_set&);
    bool xProcessInputObject(
        CSeq_align&);
    bool xProcessInputObject(
        CSeq_align_set&);

    bool xProcessBioseqHandle(
        CBioseq_Handle& bsh,
        bool skipHeaders);

    unsigned int xGffFlags(
        const CArgs& );

    TSeqPos xGetFrom(
        const CArgs& args) const;

    TSeqPos xGetTo(
        const CArgs& args) const;

    string xAssemblyName() const;
    string xAssemblyAccession() const;
    void xTweakAnnotSelector(
        const CArgs&,
        SAnnotSelector&);

    static void xReadObject(
        CObjectIStream& istr,
        CSerialObject& object); //throws

    bool m_Cleanup {false};
    CRef<CObjectManager> m_pObjMngr;
    CRef<CScope> m_pScope;
    CRef<CWriterBase> m_pWriter;
    unique_ptr<CAnnotWriterLogger> m_pErrorHandler;
};

CAnnotWriterApp::CAnnotWriterApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}

//  ----------------------------------------------------------------------------
void CAnnotWriterApp::Init()
//  ----------------------------------------------------------------------------
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Convert ASN.1 to alternative file formats");

    // input
    {{
        arg_desc->AddOptionalKey( "i", "InputFile",
            "Input file name", CArgDescriptions::eInputFile );
        arg_desc->AddOptionalKey( "id", "InputID",
            "Input ID (accession or GI number)", CArgDescriptions::eString );
        arg_desc->AddOptionalKey( "ids", "InputIDs",
            "Comma separated list of Input IDs (accession or GI number)",
            CArgDescriptions::eString );
    }}

    // format
    {{
    arg_desc->AddDefaultKey(
        "format",
        "STRING",
        "Output format",
        CArgDescriptions::eString,
        "gff3" );
    arg_desc->SetConstraint(
        "format",
        &(*new CArgAllow_Strings,
            "gvf",
            "gff3",
            "gtf",
            "wig", "wiggle",
            "bed",
            "bedgraph",
            "vcf",
            "aln",
            "psl"));
    }}

    // parameters
    {{
    arg_desc->AddDefaultKey(
        "assembly-name",
        "STRING",
        "Assembly name to include in GFF3 pragma",
        CArgDescriptions::eString,
        "" );
    arg_desc->AddDefaultKey(
        "assembly-accn",
        "STRING",
        "Assembly accession to include in GFF3 pragma",
        CArgDescriptions::eString,
        "" );
    arg_desc->AddDefaultKey(
        "default-method",
        "STRING",
        "Default term to use in GFF3 column 2",
        CArgDescriptions::eString,
        "" );
    }}


    // output
    {{
        arg_desc->AddOptionalKey( "o", "OutputFile",
            "Output file name", CArgDescriptions::eOutputFile );
        arg_desc->AddOptionalKey("from", "From",
            "Beginning of range", CArgDescriptions::eInteger );
        arg_desc->AddOptionalKey("to", "To",
            "End of range", CArgDescriptions::eInteger );
    }}

    // filters
    {{
        arg_desc->AddDefaultKey(
            "view",
            "STRING",
            "Mol filter",
            CArgDescriptions::eString,
            "nucs-only");
        arg_desc->SetConstraint(
            "view",
            &(*new CArgAllow_Strings,
                "nucs-only",
                "prots-only",
                "all"));
    }}

    //  flags
    {{
    arg_desc->AddFlag(
        "throw-exception-on-unresolved-gi",
        "throw if a gi cannot be converted to accession.version",
        true);
    arg_desc->AddFlag(
        "debug-output",
        "produce formally invalid output that is easy to read",
        true );
    arg_desc->AddFlag(
        "structibutes",
        "limit attributes to inter feature relationships",
        true );
    arg_desc->AddFlag(
        "skip-gene-features",
        "GTF only: Do not emit gene features (for GTF 2.2 compliancy)",
        true );
    arg_desc->AddFlag(
        "skip-exon-numbers",
        "GTF only: For exon features, do not emit exon numbers",
        true );
    arg_desc->AddFlag(
        "skip-headers",
        "GFF dialects: Do not generate GFF headers",
        true );
    arg_desc->AddFlag(
        "full-annots",
        "GFF dialects: Inherit annots from components, unless prohibited",
        true );
    arg_desc->AddFlag(
        "use-extra-quals",
        "GFF dialects: Restore original GFF type and attributes, if possible",
        true );
    arg_desc->AddFlag(
        "generate-missing-transcripts",
        "GFF dialects: Generate artificial transcript features for CDS features without one",
        true );
    arg_desc->AddFlag(
        "flybase",
        "GFF3 only: Use Flybase interpretation of the GFF3 spec",
        true );
    arg_desc->AddFlag(
        "no-sort",
        "GFF3 alignments only: Do not sort alignments when fetching from ID",
        true);
    arg_desc->AddFlag(
        "micro-introns",
        "GFF3 only: Incorporate micro introns",
        true);
    arg_desc->AddFlag(
        "binary",
        "input file is binary ASN.1",
        true );

    // no-cleanup
    arg_desc->AddFlag("nocleanup",
            "Do not perform data cleanup prior to formatting GTF and GFF3");

    }}

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, default_loaders);
    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();
    auto format = args["format"].AsString();
    m_Cleanup = !args["nocleanup"] && (format == "gtf" || format == "gff3");

    CONNECT_Init(&GetConfig());
    m_pObjMngr = CObjectManager::GetInstance();

    CDataLoadersUtil::SetupObjectManager(args, *m_pObjMngr, default_loaders);

    m_pScope.Reset(new CScope(*m_pObjMngr));
    m_pScope->AddDefaults();

    m_pErrorHandler.reset(new CAnnotWriterLogger);

    try {
        CNcbiOstream* pOs = xInitOutputStream(args);
        m_pWriter.Reset(xInitWriter(args, pOs));

        if (args["throw-exception-on-unresolved-gi"].AsBoolean()) {
            m_pWriter->SetIdResolve().SetThrowOnUnresolvedGi(true);
        }
#if defined(CANCELER_CODE)
        TestCanceler canceller;
        m_pWriter->SetCanceler(&canceller);
#endif
        if (args["from"]) {
            m_pWriter->SetRange().SetFrom(xGetFrom(args));
        }

        if (args["to"]) {
            m_pWriter->SetRange().SetTo(xGetTo(args));
        }

        if (xTryProcessInputIdList(args)) {
            pOs->flush();
            return 0;
        }
        if (xTryProcessInputId(args)) {
            pOs->flush();
            return 0;
        }
        if (xTryProcessInputFile(args)) {
            pOs->flush();
            return 0;
        }
    }
    catch(CWriterMessage& writerMessage) {
        m_pErrorHandler->DumpMessage(writerMessage, cerr);
        return 1;
    }
    catch (CException& e) {
        CWriterMessage writerMessage(
            "system: Exception thrown [" + e.GetMsg() + "]", eDiag_Fatal);
        m_pErrorHandler->DumpMessage(writerMessage, cerr);
        return 1;
    }
    return 0;
}

//  -----------------------------------------------------------------------------
CNcbiOstream* CAnnotWriterApp::xInitOutputStream(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (args["o"]) {
        try {
            return &args["o"].AsOutputFile();
        }
        catch(CException& e) {
            CWriterMessage writerMessage(
                "annotwriter: Unable to open output stream [" +
                    e.GetMsg() + "]", eDiag_Fatal);
            throw writerMessage;
        }
    }
    return &cout;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputId(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["id"]) {
        return false;
    }
    return xTryProcessInputId(args["id"].AsString(), args["skip-headers"]);
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputId(
    const string& id, bool skipHeaders)
//  -----------------------------------------------------------------------------
{
    auto pScope = Ref(new CScope(*m_pObjMngr));
    pScope->AddDefaults();

    CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(id);
    CBioseq_Handle bsh = pScope->GetBioseqHandle(seqh);
    if (!bsh) {
        return false;
    }

    auto pEntry = Ref(new CSeq_entry());
    pEntry->Assign(*(bsh.GetTopLevelEntry().GetCompleteSeq_entry()));
    auto tseh = m_pScope->AddTopLevelSeqEntry(*pEntry);

    bsh = m_pScope->GetBioseqHandle(seqh); // bsh now refers to local record
    auto result = xProcessBioseqHandle(bsh, skipHeaders);
    m_pScope->RemoveEntry(*pEntry);

    return result;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputIdList(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["ids"]) {
        return false;
    }
    vector<string> inputIds;
    NStr::Split(args["ids"].AsString(), ",", inputIds);
    const bool skipHeaders = args["skip-headers"];
    for (const auto& inputId: inputIds) {
        xTryProcessInputId(inputId, skipHeaders);
    }
    return true;
}


//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputFile(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(args["id"]) {
        return false;
    }

    CFileContentInfo contentInfo;
    unique_ptr<CObjectIStream> pIs(xInitInputStream(args, contentInfo));
    string genbankType = contentInfo.mInfoGenbank.mObjectType;
    auto& instr(*pIs);

    while (!instr.EndOfData()) {
        if (genbankType == "Seq-entry") {
            CRef<CSeq_entry> pObject(new CSeq_entry);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Seq-annot") {
            CRef<CSeq_annot> pObject(new CSeq_annot);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Bioseq") {
            CRef<CBioseq> pObject(new CBioseq);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Bioseq-set") {
            CRef<CBioseq_set> pObject(new CBioseq_set);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Seq-align") {
            CRef<CSeq_align> pObject(new CSeq_align);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Seq-align-set") {
            CRef<CSeq_align_set> pObject(new CSeq_align_set);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
        if (genbankType == "Seq-submit") {
            CRef<CSeq_submit> pObject(new CSeq_submit);
            xReadObject(instr, *pObject);
            xProcessInputObject(*pObject);
            continue;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
void CAnnotWriterApp::xReadObject(
    CObjectIStream& istr,
    CSerialObject& object)
//  ----------------------------------------------------------------------------
{
    try {
        istr >> object;
    }
    catch (CException& e) {
        CWriterMessage writerMessage(
            "annotwriter: Unable to extract object from input stream [" +
                e.GetMsg() + "]", eDiag_Fatal);
        throw writerMessage;
    }
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_submit& submit)
//  -----------------------------------------------------------------------------
{
    auto& data = submit.SetData();
    if (data.IsEntrys()) {
        if (!GetArgs()["skip-headers"]) {
            m_pWriter->WriteHeader();
        }
        for (auto pEntry : data.SetEntrys()) {
            xProcessInputObject(*pEntry, true);
        }
        return true;
    }

    if (data.IsAnnots()) {
        if (!GetArgs()["skip-headers"]) {
            m_pWriter->WriteHeader();
        }
        for (auto pAnnot : data.SetAnnots()) {
            if (m_Cleanup) {
                CCleanup cleanup;
                cleanup.BasicCleanup(*pAnnot);
            }
            m_pWriter->WriteAnnot(*pAnnot, xAssemblyName(), xAssemblyAccession());
            m_pWriter->WriteFooter();
        }
        return true;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_entry& entry)
//  -----------------------------------------------------------------------------
{
    return xProcessInputObject(entry, GetArgs()["skip-headers"]);
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_entry& entry, bool skipHeaders)
//  -----------------------------------------------------------------------------
{
    CSeq_entry_Handle seh = m_pScope->AddTopLevelSeqEntry(entry);
    if (m_Cleanup) { // cleanup annotations
        CCleanup cleanup;
        for (CSeq_annot_CI cit(seh); cit; ++cit) {
            CSeq_annot_Handle sah = *cit;
            cleanup.BasicCleanup(sah);
        }
    }

    if (!skipHeaders) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteSeqEntryHandle(seh, xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    m_pScope->RemoveEntry(entry);
    return true;
}



//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_annot& annot)
//  -----------------------------------------------------------------------------
{
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader(annot);
    }
    if (m_Cleanup) {
        CCleanup cleanup;
        cleanup.BasicCleanup(annot);
    }
    m_pWriter->WriteAnnot(annot, xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    return true;
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(CBioseq& bioseq)
//  ----------------------------------------------------------------------------
{
    return xProcessInputObject(bioseq, GetArgs()["skip-headers"]);
}

//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CBioseq& bioseq, bool skipHeaders)
//  -----------------------------------------------------------------------------
{
    CBioseq_Handle bsh = m_pScope->AddBioseq(bioseq);
    xProcessBioseqHandle(bsh, skipHeaders);
    m_pScope->RemoveBioseq(bsh);
    return true;
}


bool CAnnotWriterApp::xProcessBioseqHandle(CBioseq_Handle& bsh, bool skipHeaders)
//  -----------------------------------------------------------------------------
{
    if (m_Cleanup) { // cleanup annotations
        CCleanup cleanup;
        for (CSeq_annot_CI cit(bsh); cit; ++cit) {
            CSeq_annot_Handle sah = *cit;
            cleanup.BasicCleanup(sah);
        }
    }

    if (!skipHeaders) {
        m_pWriter->WriteHeader();
    }
    CFeatureGenerator::CreateMicroIntrons(*m_pScope, bsh, "", 0, true);
    if (!skipHeaders) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteBioseqHandle(bsh, xAssemblyName(), xAssemblyAccession() );
    m_pWriter->WriteFooter();
    return true;
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CBioseq_set& bioset)
//  -----------------------------------------------------------------------------
{
    CSeq_entry se;
    se.SetSet(bioset);
    return xProcessInputObject(se);
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_align& align)
//  -----------------------------------------------------------------------------
{
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }

    m_pWriter->WriteAlign( align, xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    return true;
}


//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::xProcessInputObject(
    CSeq_align_set& alignset)
//  -----------------------------------------------------------------------------
{
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    bool first = true;
    ITERATE(CSeq_align_set::Tdata, align_iter, alignset.Get()) {
        if(first && !GetArgs()["skip-headers"]) {
            m_pWriter->WriteAlign( **align_iter, xAssemblyName(), xAssemblyAccession());
            first = false;
        } else {
            m_pWriter->WriteAlign( **align_iter);
        }
    }
    m_pWriter->WriteFooter();
    return true;
}


//  -----------------------------------------------------------------------------
CObjectIStream* CAnnotWriterApp::xInitInputStream(
    const CArgs& args,
    CFileContentInfo& contentInfo)
//  -----------------------------------------------------------------------------
{
    CNcbiIstream* pInputStream = &NcbiCin;
    bool bDeleteOnClose = false;
    if (args["i"]) {
        const char* infile = args["i"].AsString().c_str();
        pInputStream = new CNcbiIfstream(infile, ios::binary);
        bDeleteOnClose = true;
    }

    CFormatGuessEx FG(*pInputStream);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    FG.GetFormatHints().DisableAllNonpreferred();
    CFormatGuess::EFormat inFormat = FG.GuessFormatAndContent(contentInfo);;

    ESerialDataFormat serial = eSerial_AsnText;
    switch(inFormat) {
    default:
        throw CWriterMessage(
            "annotwriter: Unsupported input format", eDiag_Fatal);
    case CFormatGuess::eTextASN:
        // serial = eSerial_AsnText;
        break;
    case CFormatGuess::eBinaryASN:
        serial = eSerial_AsnBinary;
        break;
    }

    CObjectIStream* pI = CObjectIStream::Open(
        serial, *pInputStream, (bDeleteOnClose ? eTakeOwnership : eNoOwnership));
    if (!pI) {
        throw CWriterMessage(
            "annotwriter: Unable to open input file", eDiag_Fatal);
    }
    return pI;
}


//  -----------------------------------------------------------------------------
unsigned int CAnnotWriterApp::xGffFlags(
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
   unsigned int eFlags = CGff2Writer::fNormal;
    if (args["structibutes"]) {
        eFlags |= CGtfWriter::fStructibutes;
    }
    if (args["skip-gene-features"]) {
        eFlags |= CGtfWriter::fNoGeneFeatures;
    }
    if ( args["skip-exon-numbers"] ) {
        eFlags |= CGtfWriter::fNoExonNumbers;
    }
    if ( args["use-extra-quals"] ) {
        eFlags |= CGff3Writer::fExtraQuals;
    }
    if (args["micro-introns"]) {
        eFlags |= CGff3Writer::fMicroIntrons;
    }
    auto viewType = args["view"].AsString();
    if (viewType == "all") {
        eFlags |= CGff3Writer::fIncludeProts;
    }
    if (viewType == "prots-only") {
        eFlags |= CGff3Writer::fIncludeProts;
        eFlags |= CGff3Writer::fExcludeNucs;
    }
    if (args["generate-missing-transcripts"]) {
        eFlags |= CGff2Writer::fGenerateMissingTranscripts;
    }
    return eFlags;
}

//  ----------------------------------------------------------------------------
TSeqPos CAnnotWriterApp::xGetFrom(
    const CArgs& args) const
//  ----------------------------------------------------------------------------
{
    if (args["from"]) {
        return static_cast<TSeqPos>(args["from"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeFrom();
}


//  ----------------------------------------------------------------------------
TSeqPos CAnnotWriterApp::xGetTo(
    const CArgs& args) const
//  ----------------------------------------------------------------------------
{
    if (args["to"]) {
        return static_cast<TSeqPos>(args["to"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeTo();
}


//  ----------------------------------------------------------------------------
CWriterBase* CAnnotWriterApp::xInitWriter(
    const CArgs& args,
    CNcbiOstream* pOs )
//  ----------------------------------------------------------------------------
{
    if (!m_pScope  || !pOs) {
        throw CWriterMessage(
            "annotwriter: Format writer object needs valid scope and output stream",
            eDiag_Fatal);
    }

    const string strFormat = args["format"].AsString();
    if (strFormat == "gff3") {
        const bool sortAlignments = args["no-sort"] ? false : true;
        if (args["flybase"]) {
            CGff3FlybaseWriter* pWriter = new CGff3FlybaseWriter(*m_pScope, *pOs, sortAlignments);
            return pWriter;
        }
        auto gffFlags = xGffFlags(args);
        CGff3Writer* pWriter = new CGff3Writer(*m_pScope, *pOs, gffFlags, sortAlignments);
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        if (args["default-method"]) {
            pWriter->SetDefaultMethod(args["default-method"].AsString());
        }
        return pWriter;
    }
    if (strFormat == "gtf") {
        auto gffFlags = xGffFlags(args);
        CGtfWriter* pWriter = new CGtfWriter(*m_pScope, *pOs, gffFlags);
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        return pWriter;
    }
    if (strFormat == "gvf") {
        CGvfWriter* pWriter = new CGvfWriter( *m_pScope, *pOs, xGffFlags(args));
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        return pWriter;
    }
    if (strFormat == "wiggle"  ||  strFormat == "wig") {
        return new CWiggleWriter(*m_pScope, *pOs, 0);
    }
    if (strFormat == "bed") {
        return new CBedWriter(*m_pScope, *pOs, 12);
    }
    if (strFormat == "bedgraph") {
        return new CBedGraphWriter(*m_pScope, *pOs, 4);
    }
    if (strFormat == "vcf") {
        return new CVcfWriter(*m_pScope, *pOs);
    }
    if (strFormat == "aln") {
        return new CAlnWriter(*m_pScope, *pOs, 0);
    }
    if (strFormat == "psl") {
        unsigned int flags = 0;
        if (args["debug-output"].AsBoolean()) {
            flags |= CPslWriter::fDebugOutput;
        }
        CWriterBase* pWriter = new CPslWriter(*m_pScope, *pOs, flags);
        pWriter->SetMessageListener(m_pErrorHandler.get());
        return pWriter;
    }
    throw CWriterMessage(
        "annotwriter: No suitable writer for format \"" + strFormat + "\"",
        eDiag_Fatal);
}

//  ----------------------------------------------------------------------------
string CAnnotWriterApp::xAssemblyName() const
//  ----------------------------------------------------------------------------
{
    return GetArgs()["assembly-name"].AsString();
}

//  ----------------------------------------------------------------------------
string CAnnotWriterApp::xAssemblyAccession() const
//  ----------------------------------------------------------------------------
{
    return GetArgs()["assembly-accn"].AsString();
}

//  ---------------------------------------------------------------------------
void CAnnotWriterApp::xTweakAnnotSelector(
    const CArgs& args,
    SAnnotSelector& sel)
//  ---------------------------------------------------------------------------
{
   if (args["full-annots"]) {
        sel.SetResolveDepth(kMax_Int);
        sel.SetResolveAll().SetAdaptiveDepth();
    }
    else {
        sel.SetAdaptiveDepth(false);
        sel.SetExactDepth(true);
        sel.SetResolveDepth(0);
    }
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
    return CAnnotWriterApp().AppMain(argc, argv);
}
