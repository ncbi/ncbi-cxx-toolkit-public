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
* Author:  Frank Ludwig, Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/idmapper.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Date.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include <corelib/ncbistre.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

//#include <objtools/readers/error_container.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

void GetSeqId(CRef<CSeq_id>& id, const CTable2AsnContext& context)
{
    if (context.m_SetIDFromFile)
    {
        string base;
        CDirEntry::SplitPath(context.m_current_file, 0, &base, 0);
        id.Reset(new CSeq_id(string("lcl|") + base));
    }
}

}
//  ============================================================================
void DumpMemory(const string& prefix)
    //  ============================================================================
{
    Uint8 totalMemory = GetPhysicalMemorySize();
    size_t usedMemory; size_t residentMemory; size_t sharedMemory;
    if (!GetMemoryUsage(&usedMemory, &residentMemory, &sharedMemory)) {
        cerr << "Unable to get memory counts!" << endl;
    }
    else {
        cerr << prefix
            << "Total:" << totalMemory
            << " Used:" << usedMemory << "("
            << (100*usedMemory)/totalMemory <<"%)"
            << " Resident:" << residentMemory << "("
            << int((100.0*residentMemory)/totalMemory) <<"%)"
            << endl;
    }
}

#if 0
//  ============================================================================
class CErrorContainerCustom:
    //  ============================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerCustom(
        int iMaxCount,
        int iMaxLevel ): m_iMaxCount( iMaxCount ), m_iMaxLevel( iMaxLevel ) {};
    ~CErrorContainerCustom() {};

    bool
        PutError(
        const ILineError& err )
    {
        StoreError(err);
        return (err.Severity() <= m_iMaxLevel) && (Count() < m_iMaxCount);
    };

protected:
    size_t m_iMaxCount;
    int m_iMaxLevel;
};
#endif


#if 0
//  ----------------------------------------------------------------------------
CArgDescriptions* CMultiReader::InitAppArgs(CNcbiApplication& app)
    //  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(app.GetArguments().GetProgramBasename(), "C++ multi format file reader");

    //
    //  input / output:
    //
    arg_desc->AddOptionalKey(
        "input",
        "File_In",
        "Input filename",
        CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey(
        "output",
        "File_Out",
        "Output filename",
        CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey(
        "format",
        "STRING",
        "Input file format",
        CArgDescriptions::eString,
        "guess");
    arg_desc->SetConstraint(
        "format",
        &(*new CArgAllow_Strings,
        "bed",
        "microarray", "bed15",
        "wig", "wiggle",
        "gtf", "gff3", "gff2",
        "gvf",
        "agp",
        "newick", "tree", "tre",
        "vcf",
        "aln", "align",
        "hgvs",
        "fasta",
        "5colftbl",
        "guess") );

    arg_desc->AddDefaultKey(
        "flags",
        "INTEGER",
        "Additional flags passed to the reader",
        CArgDescriptions::eString,
        "0" );

    arg_desc->AddDefaultKey(
        "name",
        "STRING",
        "Name for annotation",
        CArgDescriptions::eString,
        "");
    arg_desc->AddDefaultKey(
        "title",
        "STRING",
        "Title for annotation",
        CArgDescriptions::eString,
        "");

    //
    //  ID mapping:
    //
    arg_desc->AddDefaultKey(
        "mapfile",
        "File_In",
        "IdMapper config filename",
        CArgDescriptions::eString, "" );

    arg_desc->AddDefaultKey(
        "genome",
        "STRING",
        "UCSC build number",
        CArgDescriptions::eString,
        "" );

    //
    //  Error policy:
    //
    arg_desc->AddFlag(
        "dumpstats",
        "write record counts to stderr",
        true );

    arg_desc->AddFlag(
        "checkonly",
        "check for errors only",
        true );

    arg_desc->AddFlag(
        "noerrors",
        "suppress error display",
        true );

    arg_desc->AddFlag(
        "lenient",
        "accept all input format errors",
        true );

    arg_desc->AddFlag(
        "strict",
        "accept no input format errors",
        true );

    arg_desc->AddDefaultKey(
        "max-error-count",
        "INTEGER",
        "Maximum permissible error count",
        CArgDescriptions::eInteger,
        "-1" );

    arg_desc->AddDefaultKey(
        "max-error-level",
        "STRING",
        "Maximum permissible error level",
        CArgDescriptions::eString,
        "warning" );

    arg_desc->SetConstraint(
        "max-error-level",
        &(*new CArgAllow_Strings,
        "info", "warning", "error" ) );

    //
    //  bed and gff reader specific arguments:
    //
    arg_desc->AddFlag(
        "all-ids-as-local",
        "turn all ids into local ids",
        true );

    arg_desc->AddFlag(
        "numeric-ids-as-local",
        "turn integer ids into local ids",
        true );

    //
    //  wiggle reader specific arguments:
    //
    arg_desc->AddFlag(
        "join-same",
        "join abutting intervals",
        true );

    arg_desc->AddFlag(
        "as-byte",
        "generate byte compressed data",
        true );

    arg_desc->AddFlag(
        "as-graph",
        "generate graph object",
        true );

    arg_desc->AddFlag(
        "raw",
        "iteratively return raw track data",
        true );

    //
    //  gff reader specific arguments:
    //
    arg_desc->AddFlag( // no longer used, retained for backward compatibility
        "new-code",
        "use new gff3 reader implementation",
        true );
    arg_desc->AddFlag(
        "old-code",
        "use old gff3 reader implementation",
        true );

    //
    //  alignment reader specific arguments:
    //
    arg_desc->AddDefaultKey(
        "aln-gapchar",
        "STRING",
        "Alignment gap character",
        CArgDescriptions::eString,
        "-");

    arg_desc->AddDefaultKey(
        "aln-alphabet",
        "STRING",
        "Alignment alphabet",
        CArgDescriptions::eString,
        "nuc");
    arg_desc->SetConstraint(
        "aln-alphabet",
        &(*new CArgAllow_Strings,
        "nuc",
        "prot") );

    arg_desc->AddFlag(
        "force-local-ids",
        "treat all IDs as local IDs",
        true);

    //app.SetupArgDescriptions(arg_desc.release());
    return arg_desc.release();
}
#endif


CRef<CSeq_entry> CMultiReader::xReadFile(const string& ifname)
{
    CNcbiIfstream istr(ifname.c_str());

    xSetFormat(m_context, istr);
    xSetFlags(m_context, istr);
    xSetMapper(m_context);
    xSetErrorContainer(m_context);

    CRef<CSeq_entry> result;
    switch( m_uFormat )
    {
    case CFormatGuess::eTextASN:
        result = xReadASN1(istr);
        break;
    default:
        result = xReadFasta(istr);
        break;
    }
    return result;
}

CRef<CSeq_entry>
CMultiReader::xReadASN1(CNcbiIstream& instream)
{
    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(instream);

    // guess object type
    const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CSeq_submit::GetTypeInfo()->GetName() ) {
        CRef<CSeq_submit> submit (new CSeq_submit);
        pObjIstrm->Read(ObjectInfo(*submit), CObjectIStream::eNoFileHeader);

        CRef<CSeq_entry> entry(new CSeq_entry);
        if (submit->GetData().GetEntrys().size() > 1)
        {
            entry.Reset(new CSeq_entry);
            entry->SetSet().SetSeq_set() = submit->GetData().GetEntrys();
        }
        else
            entry = *submit->SetData().SetEntrys().begin(); 

        return entry;
    }
    else
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
        CRef<CSeq_entry> entry (new CSeq_entry);
        pObjIstrm->Read(ObjectInfo(*entry), CObjectIStream::eNoFileHeader);

        return entry;
    }

    return CRef<CSeq_entry>();
}

//  ----------------------------------------------------------------------------
CRef<CSeq_entry> 
CMultiReader::xReadFasta(CNcbiIstream& instream)
    //  ----------------------------------------------------------------------------
{
    if (m_context.m_gapNmin > 0)
    {
        m_iFlags |= CFastaReader::fParseGaps 
                 |  CFastaReader::fLetterGaps;
    }
    else
    {
        m_iFlags |= CFastaReader::fNoSplit;
//                 |  CFastaReader::fLeaveAsText;
    }
    m_iFlags |= CFastaReader::fAddMods
             |  CFastaReader::fAssumeNuc
             |  CFastaReader::fValidate;

    auto_ptr<CFastaReader> pReader(new CFastaReader(0, m_iFlags));   
    if (!pReader.get()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "File format not supported", 0);
    }

    CStreamLineReader lr( instream );
    CRef<CSeq_entry> result = pReader->ReadSeqEntry (lr, m_context.m_logger);
    if (result->IsSet() && !m_context.m_HandleAsSet)
    {
        m_context.m_logger->PutError(
            *CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0,
            "File " + m_context.m_current_file + " contains multiple sequences"));
    }
    return result;

}

//  ----------------------------------------------------------------------------
void CMultiReader::xSetFormat(
    const CTable2AsnContext& args,
    CNcbiIstream& istr )
    //  ----------------------------------------------------------------------------
{
    m_uFormat = CFormatGuess::eUnknown;
#if 0
    const string& strProgramName = "GetProgramDisplayName";

    if (NStr::StartsWith(strProgramName, "wig") || format == "wig" ||
        format == "wiggle") {
            m_uFormat = CFormatGuess::eWiggle;
    }
    if (NStr::StartsWith(strProgramName, "bed") || format == "bed") {
        m_uFormat = CFormatGuess::eBed;
    }
    if (NStr::StartsWith(strProgramName, "b15") || format == "bed15" ||
        format == "microarray") {
            m_uFormat = CFormatGuess::eBed15;
    }
    if (NStr::StartsWith(strProgramName, "gtf") || format == "gtf") {
        m_uFormat = CFormatGuess::eGtf;
    }
    if (NStr::StartsWith(strProgramName, "gff") ||
        format == "gff3" || format == "gff2") {
            m_uFormat = CFormatGuess::eGff3;
    }
    if (NStr::StartsWith(strProgramName, "agp")) {
        m_uFormat = CFormatGuess::eAgp;
    }

    if (NStr::StartsWith(strProgramName, "newick") ||
        format == "newick" || format == "tree" || format == "tre") {
            m_uFormat = CFormatGuess::eNewick;
    }
    if (NStr::StartsWith(strProgramName, "gvf") || format == "gvf") {
        m_uFormat = CFormatGuess::eGtf;
    }
    if (NStr::StartsWith(strProgramName, "aln") || format == "align" ||
        format == "aln") {
            m_uFormat = CFormatGuess::eAlignment;
    }
    if (NStr::StartsWith(strProgramName, "hgvs") ||
        format == "hgvs") {
            m_uFormat = CFormatGuess::eHgvs;
    }
    if( NStr::StartsWith(strProgramName, "fasta") ||
        format == "fasta" ) {
            m_uFormat = CFormatGuess::eFasta;
    }
    if( NStr::StartsWith(strProgramName, "feattbl") ||
        format == "5colftbl" ) {
            m_uFormat = CFormatGuess::eFiveColFeatureTable;
    }
#endif
    if (m_uFormat == CFormatGuess::eUnknown) {
        m_uFormat = CFormatGuess::Format(istr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xSetFlags(
    const CTable2AsnContext& args,
    CNcbiIstream& istr)
    //  ----------------------------------------------------------------------------
{
    m_iFlags = 0;
#if 0
    if (m_uFormat == CFormatGuess::eUnknown) {
        xSetFormat(args, istr);
    }
    m_iFlags = NStr::StringToInt(
        args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );

    switch( m_uFormat ) {

    case CFormatGuess::eWiggle:
        if ( args["join-same"] ) {
            m_iFlags |= CWiggleReader::fJoinSame;
        }
        //by default now. But still tolerate if explicitly specified.
        m_iFlags |= CWiggleReader::fAsByte;

        if ( args["as-graph"] ) {
            m_iFlags |= CWiggleReader::fAsGraph;
        }

        if ( args["raw"] ) {
            m_iFlags |= CReaderBase::fAsRaw;
        }
        break;

    case CFormatGuess::eBed:
        if ( args["all-ids-as-local"] ) {
            m_iFlags |= CBedReader::fAllIdsAsLocal;
        }
        if ( args["numeric-ids-as-local"] ) {
            m_iFlags |= CBedReader::fNumericIdsAsLocal;
        }
        if ( args["raw"] ) {
            m_iFlags |= CReaderBase::fAsRaw;
        }
        break;

    case CFormatGuess::eGtf:
        if ( args["all-ids-as-local"] ) {
            m_iFlags |= CGFFReader::fAllIdsAsLocal;
        }
        if ( args["numeric-ids-as-local"] ) {
            m_iFlags |= CGFFReader::fNumericIdsAsLocal;
        }

    default:
        break;
    }
    m_AnnotName = args["name"].AsString();
    m_AnnotTitle = args["title"].AsString();
#endif
    m_iFlags |= objects::CFastaReader::fNoUserObjs;
    //m_bCheckOnly = false; //args["checkonly"];
}

//  ----------------------------------------------------------------------------
void CMultiReader::WriteObject(
    CSerialObject& object,                  // potentially modified by mapper
    ostream& ostr)
    //  ----------------------------------------------------------------------------
{
    if (m_pMapper.get()) {
        m_pMapper->MapObject(object);
    }
    ostr << MSerial_AsnText 
         //<< MSerial_VerifyNo
         << object;
    ostr.flush();
}

//  ----------------------------------------------------------------------------
void CMultiReader::xSetMapper(const CTable2AsnContext& args)
    //  ----------------------------------------------------------------------------
{
#if 0
    string strBuild = args["genome"].AsString();
    string strMapFile = args["mapfile"].AsString();

    if (strBuild.empty() && strMapFile.empty()) {
        return;
    }
    if (!strMapFile.empty()) {
        CNcbiIfstream* pMapFile = new CNcbiIfstream(strMapFile.c_str());
        m_pMapper.reset(
            new CIdMapperConfig(*pMapFile, strBuild, false, m_pErrors));
    }
    else {
        m_pMapper.reset(new CIdMapperBuiltin(strBuild, false, m_pErrors));
    }
#endif
}

//  ----------------------------------------------------------------------------
void CMultiReader::xSetErrorContainer(const CTable2AsnContext& args)
    //  ----------------------------------------------------------------------------
{
#if 0
    //
    //  By default, allow all errors up to the level of "warning" but nothing
    //  more serious. -strict trumps everything else, -lenient is the second
    //  strongest. In the absence of -strict and -lenient, -max-error-count and
    //  -max-error-level become additive, i.e. both are enforced.
    //
    if ( args["noerrors"] ) {   // not using error policy at all
        m_pErrors = 0;
        return;
    }
    if ( args["strict"] ) {
        m_pErrors = new CErrorContainerStrict;
        return;
    }
    if ( args["lenient"] ) {
        m_pErrors = new CErrorContainerLenient;
        return;
    }

    int iMaxErrorCount = args["max-error-count"].AsInteger();
    int iMaxErrorLevel = eDiag_Error;
    string strMaxErrorLevel = args["max-error-level"].AsString();
    if ( strMaxErrorLevel == "info" ) {
        iMaxErrorLevel = eDiag_Info;
    }
    else if ( strMaxErrorLevel == "error" ) {
        iMaxErrorLevel = eDiag_Error;
    }

    if ( iMaxErrorCount == -1 ) {
        m_pErrors.Reset(new CErrorContainerLevel(iMaxErrorLevel));
        return;
    }
    m_pErrors.Reset(new CErrorContainerCustom(iMaxErrorCount, iMaxErrorLevel));
#endif
}

CMultiReader::CMultiReader(const CTable2AsnContext& context)
    :m_context(context)
{
}

void CMultiReader::ApplyAdditionalProperties(CSeq_entry& entry)
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        if (m_context.m_SetIDFromFile)
        {
            CRef<CSeq_id> id;
            GetSeqId(id, m_context);
            entry.SetSeq().SetId().push_back(id);
        }

        break;
    case CSeq_entry::e_Set:
        {
            if (m_context.m_SetIDFromFile)
            {
                CRef<CSeq_id> id;
                GetSeqId(id, m_context);
                entry.SetSet().SetId().Assign(*id);
            }
            if (!entry.SetSet().IsSetClass())
            {
                entry.SetSet().SetClass(CBioseq_set_Base::eClass_genbank);
            }
#if 0
            if (m_context.m_GenomicProductSet)
            {
                //entry.SetSet().SetClass(CBioseq_set_Base::eClass_gen_prod_set);
            }
            if (m_context.m_NucProtSet)
            {
                //entry.SetSet().SetClass(CBioseq_set_Base::eClass_nuc_prot);
            }
#endif
            NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
            {
                ApplyAdditionalProperties(**it);
            }
        }
        break;
    default:
        break;
    }

    if (!m_context.m_Comment.empty())
    {
        CRef<CSeqdesc> value(new CSeqdesc());
        value->SetComment(m_context.m_Comment);

        MergeDescriptors(entry.SetDescr(), *value);
    }

    if (!m_context.m_OrganismName.empty())
    {
        m_context.SetBioSource(entry.SetDescr()).SetOrg().SetTaxname(m_context.m_OrganismName);
    }
}

void CMultiReader::LoadDescriptors(const string& ifname, CRef<CSeq_descr> & out_desc)
{
    out_desc.Reset(new CSeq_descr);

    CNcbiIfstream istrm(ifname.c_str());

    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(istrm);

    // guess object type
    //const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    while (true) {
        try {
            const string sType = pObjIstrm->ReadFileHeader();
            if (sType == CSeq_descr::GetTypeInfo()->GetName())
            {
                CRef<CSeq_descr> descr(new CSeq_descr);
                pObjIstrm->Read(ObjectInfo(*descr),
                    CObjectIStream::eNoFileHeader);
                out_desc->Set().insert(out_desc->Set().end(), descr->Get().begin(), descr->Get().end());
            }
            else
                if (sType == CSeqdesc::GetTypeInfo()->GetName())
                {
                    CRef<CSeqdesc> desc(new CSeqdesc);
                    pObjIstrm->Read(ObjectInfo(*desc),
                        CObjectIStream::eNoFileHeader);
                    out_desc->Set().push_back(desc);
                }
                else
                {
                    throw runtime_error("Descriptor file must contain "
                        "either Seq_descr or Seqdesc elements");
                }
        } catch (...) {
            break;
        }
    }
}

void CMultiReader::LoadTemplate(CTable2AsnContext& context, const string& ifname)
{
    context.m_entry_template.Reset( new CSeq_entry );

#if 0
    // check if the location doesn't exist, and see if we can
    // consider it some kind of sequence identifier
    if( ! CDirEntry(ifname).IsFile() ) {
        // see if this is blatantly not a sequence identifier
        if( ! CRegexpUtil(sTemplateLocation).Exists("^[A-Za-z0-9_|]+(\\.[0-9]+)?$") ) {
            throw runtime_error("This is not a valid sequence identifier: " + sTemplateLocation);
        }

        // try to load from genbank
        CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
        CRef<CScope> pScope(new CScope(*CObjectManager::GetInstance()));
        pScope->AddDefaults();

        CRef<CSeq_id> pTemplateId( new CSeq_id(sTemplateLocation) );
        CBioseq_Handle bsh = pScope->GetBioseqHandle( *pTemplateId );

        if ( ! bsh ) {
            throw runtime_error("Invalid sequence identifier: " + sTemplateLocation);
        }
        CSeq_entry_Handle entry_h = bsh.GetParentEntry();

        context.m_entry_template->Assign( *entry_h.GetCompleteSeq_entry() );
        return;
    }
#endif


    CNcbiIfstream istrm(ifname.c_str());

    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(istrm);

    // guess object type
    const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
        pObjIstrm->Read(ObjectInfo(*context.m_entry_template),
            CObjectIStream::eNoFileHeader);
    } else if( sType == CBioseq::GetTypeInfo()->GetName() ) {
        CRef<CBioseq> pBioseq( new CBioseq );
        pObjIstrm->Read(ObjectInfo(*pBioseq),
            CObjectIStream::eNoFileHeader);
        context.m_entry_template->SetSeq( *pBioseq );
    } else if( sType == CSeq_submit::GetTypeInfo()->GetName() ) {

        context.m_submit_template.Reset( new CSeq_submit );
        pObjIstrm->Read(ObjectInfo(*context.m_submit_template),
            CObjectIStream::eNoFileHeader);
        if (!context.m_submit_template->GetData().IsEntrys()
            || context.m_submit_template->GetData().GetEntrys().size() != 1)
        {
            throw runtime_error("Seq-submit template must contain "
                "exactly one Seq-entry");
        }
    } else if( sType == CSubmit_block::GetTypeInfo()->GetName() ) {

        // a Submit-block
        CRef<CSubmit_block> submit_block(new CSubmit_block);
        pObjIstrm->Read(ObjectInfo(*submit_block),
            CObjectIStream::eNoFileHeader);

        // Build a Seq-submit containing this plus a bogus Seq-entry
        context.m_submit_template.Reset( new CSeq_submit );
        context.m_submit_template->SetSub(*submit_block);
        CRef<CSeq_entry> ent(new CSeq_entry);
        CRef<CSeq_id> dummy_id(new CSeq_id("lcl|dummy_id"));
        ent->SetSeq().SetId().push_back(dummy_id);
        ent->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
        ent->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        context.m_submit_template->SetData().SetEntrys().push_back(ent);
    } else {
        NCBI_USER_THROW_FMT("Template must be Seq-entry, Seq-submit, Bioseq or "
            "Submit-block.  Object seems to be of type: " << sType);
    }

    // for submit types, pull out the seq-entry inside and remember it
    if( context.m_submit_template->IsEntrys() ) {
        context.m_entry_template = context.m_submit_template->SetData().SetEntrys().front();
    }

    // The template may contain a set rather than a seq.
    // That's OK if it contains only one na entry, which we'll use.
    if (context.m_entry_template->IsSet()) {
        unsigned int num_nuc_ents = 0;
        CRef<CSeq_entry> tmp( new CSeq_entry );
        ITERATE (CBioseq_set::TSeq_set, ent_iter,
            context.m_entry_template->GetSet().GetSeq_set()) {
                if ((*ent_iter)->GetSeq().GetInst().IsNa()) {
                    ++num_nuc_ents;
                    tmp->Assign(**ent_iter);
                    // Copy any descriptors from the set to the sequence
                    ITERATE (CBioseq_set::TDescr::Tdata, desc_iter,
                        context.m_entry_template->GetSet().GetDescr().Get()) {
                            CRef<CSeqdesc> desc(new CSeqdesc);
                            desc->Assign(**desc_iter);
                            tmp->SetSeq().SetDescr().Set().push_back(desc);
                    }
                }
        }
        if (num_nuc_ents == 1) {
            context.m_entry_template->Assign(*tmp);
        } else {
            throw runtime_error("template contains "
                + NStr::IntToString(num_nuc_ents)
                + " nuc. Seq-entrys; should contain 1");
        }
    }

    // incorporate any Seqdesc's that follow in the file
    while (true) {
        try {
            CRef<CSeqdesc> desc(new CSeqdesc);
            pObjIstrm->Read(ObjectInfo(*desc));
            context.m_entry_template->SetSeq().SetDescr().Set().push_back(desc);
        } catch (...) {
            break;
        }
    }

#if 0
    if ( context.m_submit_template->IsEntrys() ) {
        // Take Seq-submit.sub.cit and put it in the Bioseq
        CRef<CPub> pub(new CPub);
        pub->SetSub().Assign(context.m_submit_template->GetSub().GetCit());
        CRef<CSeqdesc> pub_desc(new CSeqdesc);
        pub_desc->SetPub().SetPub().Set().push_back(pub);
        context.m_entry_template->SetSeq().SetDescr().Set().push_back(pub_desc);
    }
#endif

    if( ! context.m_entry_template->IsSeq() ) {
        throw runtime_error("The Seq-entry must be a Bioseq not a Bioseq-set.");
    }

    if (context.m_submit_template.NotEmpty())
    {
        if (context.m_submit_template->IsSetSub() &&
            context.m_submit_template->GetSub().IsSetCit())
        {
        CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
        context.m_submit_template->SetSub().SetCit().SetDate(*date);
        }
    }

#if 0
    if( args["output-type"].AsString() == "Seq-entry" ) {
        // force Seq-entry by throwing out the Seq-submit
        context.m_submit_template.Reset( new CSeq_submit );
    }
#endif
}

namespace
{
    class AllowedDuplicates: public set<CSeqdesc_Base::E_Choice>
    {
    public:
        AllowedDuplicates()
        {
            insert(CSeqdesc_Base::e_User);
        }
    };
    AllowedDuplicates m_allowed_duplicates;

    template<typename _which>
    struct LocateWhich
    {
        typename _which::E_Choice compare_to;
        bool operator() (_which l)  const
        {
            return l.Which() == compare_to;
        }
        bool operator() (const CRef<_which>& l)  const
        {
            return l->Which() == compare_to;
        }
    };
}

void CMultiReader::MergeDescriptors(CSeq_descr & dest, const CSeq_descr & source)
{
    ITERATE(CSeq_descr::Tdata, it, source.Get())
    {
        MergeDescriptors(dest, **it);
    }
}

void CMultiReader::MergeDescriptors(CSeq_descr & dest, const CSeqdesc & source)
{
    if (m_allowed_duplicates.find(source.Which()) == m_allowed_duplicates.end())
    {
        LocateWhich<CSeqdesc> pred; pred.compare_to = source.Which();
        CSeq_descr::Tdata::iterator it = find_if(dest.Set().begin(), dest.Set().end(), pred);
        if (it != dest.Set().end())
            dest.Set().erase(it);
    }

    CRef<CSeqdesc> desc (new CSeqdesc);
    desc->Assign(source);
    dest.Set().push_back(desc);

}

void CMultiReader::ApplyDescriptors(objects::CSeq_entry& entry, const CSeq_descr& source)
{
    MergeDescriptors(entry.SetDescr(), source);
}

CRef<CSeq_entry> CMultiReader::CreateNewSeqFromTemplate(const CTable2AsnContext& context, CBioseq& bioseq) const 
{
    CRef<CSeq_entry> result(new CSeq_entry);
    if (context.m_entry_template.NotEmpty())
       result->Assign(*context.m_entry_template);
    result->SetSeq(bioseq);

    return result;
}

CRef<CSeq_entry> CMultiReader::LoadFile(const string& ifname)
{
    CRef<CSeq_entry> read_entry = xReadFile(ifname);
    if (read_entry.NotEmpty() && m_context.m_entry_template.NotEmpty())
    {
        m_context.MergeWithTemplate(*read_entry);
    }
    return read_entry;
}

void CMultiReader::Cleanup(CRef<CSeq_entry> entry)
{
    /*
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    changes = cleanup.ExtendedCleanup(*entry);
    */
}


#if 0
//  ----------------------------------------------------------------------------
int CMultiReader::RunOld(const CTable2AsnContext& args, const string& ifname, CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    //  const CArgs& args = GetArgs();
    //CNcbiIstream& istr = args["input"].AsInputFile();
    //CNcbiOstream& ostr = args["output"].AsOutputFile();
    //CNcbiIstream& istr = *args.m_input;
    //CNcbiOstream& ostr = *args.m_output;
    CNcbiIfstream istr(ifname.c_str());
    //CNcbiOfstream ostr(ofname);


    xSetFormat(args, istr);
    xSetFlags(args, istr);
    xSetMapper(args);
    xSetErrorContainer(args);

    //CRef< CSerialObject> object;
    //vector< CRef< CSeq_annot > > annots;
    switch( m_uFormat ) {
    default:
        xProcessDefault(args, istr);
        //ApplyAdditionalProperties(args);
        WriteObject(*m_pObject, ostr);
        break;
    case CFormatGuess::eWiggle:
        if (m_iFlags & CReaderBase::fAsRaw) {
            xProcessWiggleRaw(args, istr, ostr);
        }
        else {
            xProcessWiggle(args, istr, ostr);
        }
        break;
    case CFormatGuess::eBed:
        if (m_iFlags & CReaderBase::fAsRaw) {
            xProcessBedRaw(args, istr, ostr);
        }
        else {
            xProcessBed(args, istr, ostr);
        }
        break;
    case CFormatGuess::eGtf:
    case CFormatGuess::eGtf_POISENED:
        xProcessGtf(args, istr, ostr);
        break;
    case CFormatGuess::eVcf:
        xProcessVcf(args, istr, ostr);
        break;
    case CFormatGuess::eNewick:
        xProcessNewick(args, istr, ostr);
        break;
    case CFormatGuess::eGff3:
        xProcessGff3(args, istr, ostr);
        break;
    case CFormatGuess::eGff2:
        xProcessGff2(args, istr, ostr);
        break;
    case CFormatGuess::eGvf:
        xProcessGvf(args, istr, ostr);
        break;
    case CFormatGuess::eAgp:
        xProcessAgp(args, istr, ostr);
        break;
    case CFormatGuess::eAlignment:
        xProcessAlignment(args, istr, ostr);
        break;
    }

    xDumpErrors( cerr );
    return 0;
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessWiggle(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CWiggleReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessWiggleRaw(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CWiggleReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRawWiggleTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessBed(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    //  Use ReadSeqAnnot() over ReadSeqAnnots() to keep memory footprint down.
    CBedReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRef<CSeq_annot> pAnnot = reader.ReadSeqAnnot(lr, m_pErrors);
    while(pAnnot) {
        xWriteObject(*pAnnot, ostr);
        pAnnot.Reset();
        pAnnot = reader.ReadSeqAnnot(lr, m_pErrors);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessBedRaw(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CBedReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRawBedTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGtf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    if (args.m_format == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    CGtfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGff3(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    if (args.m_format == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    CGff3Reader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGff2(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CGff2Reader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGvf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    if (args.m_format == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    if (args.m_format == "gff3") { // process as plain GFF2
        return xProcessGff3(args, istr, ostr);
    }
    CGvfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessVcf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CVcfReader reader( m_iFlags );
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessNewick(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    string strTree;
    char c = istr.get();
    while (!istr.eof()) {
        strTree += c;
        if (c == ';') {
            CNcbiIstrstream istrstr(strTree.c_str());
            auto_ptr<TPhyTreeNode>  pTree(ReadNewickTree(istrstr) );
            CRef<CBioTreeContainer> btc = MakeBioTreeContainer(pTree.get());
            xWriteObject(*btc, ostr);
            strTree.clear();
        }
        c = istr.get();
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessAgp(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_entry> > TEntries;
    TEntries entries;
    AgpRead(istr, entries);
    NON_CONST_ITERATE (TEntries, it, entries) {
        xWriteObject(**it, ostr);
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessAlignment(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CAlnReader reader(istr);
    reader.SetAllGap(args["aln-gapchar"].AsString());
    reader.SetAlphabet(CAlnReader::eAlpha_Nucleotide);
    if (args["aln-alphabet"].AsString() == "prot") {
        reader.SetAlphabet(CAlnReader::eAlpha_Protein);
    }
    reader.Read(false, args["force-local-ids"]);
    CRef<CSeq_align> pAlign = reader.GetSeqAlign();
    xWriteObject(*pAlign, ostr);
}

void CMultiReader::xDumpErrors(CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    if (m_pErrors) {
        m_pErrors->Dump(ostr);
    }
}


#endif

auto_ptr<CObjectIStream> CMultiReader::xCreateASNStream(CNcbiIstream& instream)
{
    // guess format
    ESerialDataFormat eSerialDataFormat = eSerial_None;
    {{
        CFormatGuess::EFormat eFormat =
            CFormatGuess::Format(instream);

        switch(eFormat) {
        case CFormatGuess::eBinaryASN:
            eSerialDataFormat = eSerial_AsnBinary;
            break;
        case CFormatGuess::eTextASN:
            eSerialDataFormat = eSerial_AsnText;
            break;
        case CFormatGuess::eXml:
            eSerialDataFormat = eSerial_Xml;
            break;
        default:
            NCBI_USER_THROW_FMT(
                "Descriptor file seems to be in an unsupported format: "
                << CFormatGuess::GetFormatName(eFormat) );
            break;
        }

        instream.seekg(0);
    }}

    auto_ptr<CObjectIStream> pObjIstrm(
        CObjectIStream::Open(eSerialDataFormat, instream, eNoOwnership) );

    return pObjIstrm;
}

CMultiReader::~CMultiReader()
{
}

END_NCBI_SCOPE
