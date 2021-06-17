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
 * Authors:  Josh Cherry, Michael Kornbluh
 *
 * File Description:
 *   Read an AGP file, build Seq-entry's or Seq-submit's,
 *   and do some validation. Wrapper over CAgpConverter.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_converter.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objtools/readers/agp_seq_entry.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexec.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objmgr/scope.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <util/xregexp/regexp.hpp>
#include <util/format_guess.hpp>

#include <algorithm>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  Lookup tables

namespace {

    // some arguments simply add a subsource.
    // Adding more is very simple, just add them to sc_SimpleSubsource below
    struct SSimpleSubsourceInfo {
        const char           * m_pchSynopsis;
        const char           * m_pchUsage;
        CSubSource::ESubtype   m_eSubtype;
    };
    typedef SStaticPair<const char*, SSimpleSubsourceInfo> TSimpleSubsource;
    static const TSimpleSubsource sc_SimpleSubsource[] = { 
        {"cl",  { "clone_lib",      "Clone library (for BioSource.subtype)",          CSubSource::eSubtype_clone_lib} },
        {"cm",  { "chromosome",     "Chromosome (for BioSource.subtype)",             CSubSource::eSubtype_chromosome} },
        {"cn",  { "clone",          "Clone (for BioSource.subtype)",                  CSubSource::eSubtype_clone} },
        {"ht",  { "haplotype",      "Haplotype (for BioSource.subtype)",              CSubSource::eSubtype_haplotype} },
        {"sc",  { "source_comment", "Source comment (for BioSource.subtype = other)", CSubSource::eSubtype_other} },
        {"sex", { "sex",            "Sex/gender (for BioSource.subtype)",             CSubSource::eSubtype_sex} }
    }; 
    typedef const CStaticPairArrayMap<const char*, SSimpleSubsourceInfo, PCase_CStr> TSimpleSubsourceMap; 
    DEFINE_STATIC_ARRAY_MAP(TSimpleSubsourceMap, sc_SimpleSubsourceMap, sc_SimpleSubsource );
}

/////////////////////////////////////////////////////////////////////////////
//  CAgpconvertApplication::


class CAgpconvertApplication : public CNcbiApplication
{
public:

    CAgpconvertApplication(void) ;

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:

    class CCustomErrorHandler : public CAgpConverter::CErrorHandler
    {
    public:
        virtual void HandleError(CAgpConverter::EError eError, const string & sMessage ) const;
    };

    CRef<CCustomErrorHandler> m_pCustomErrorHandler;

    class CAsnvalRunner : public CAgpConverter::IFileWrittenCallback 
    {
    public:
        // runs asnval on the file
        virtual void Notify(const string & file);
    };


    // load the file specified via the -template arg
    void x_LoadTemplate(
        const string & sTemplateLocation,
        CRef<CSeq_entry> & out_ent_templ,
        CRef<CSeq_submit> & out_submit_templ);

    bool x_IsAnySimpleSubsourceArgSet(void);

    void x_HandleTaxArgs( CRef<CSeqdesc> source_desc );
};

/////////////////////////////////////////////////////////////////////////////
// Constructor

CAgpconvertApplication::CAgpconvertApplication(void) :
    m_pCustomErrorHandler( new CCustomErrorHandler )
{
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAgpconvertApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "AGP file converter program");

    // Describe the expected command-line arguments

    arg_desc->SetCurrentGroup("INPUT");

    arg_desc->AddKey("template", "LOCATION",
        "The filename of a template Seq-entry or Seq-submit or Bioseq in "
        "either ASN.1 text or ASN.1 binary or XML (autodetected).  A series of Seqdescs "
        "may optionally follow the main ASN.1 object."
        "Alternatively, if the LOCATION looks reasonably like a GenBank identifier and "
        "doesn't exist as a file, the template is loaded from genbank instead.",
        CArgDescriptions::eString);
    arg_desc->AddFlag("keeptemplateannots", 
        "Unless this flag is set, the annots from the template are removed");

    arg_desc->AddExtra
        (1, 32766, "AGP files to process",
         CArgDescriptions::eInputFile);

    arg_desc->SetCurrentGroup("OUTPUT");

    arg_desc->AddOptionalKey("outdir", "output_directory",
                             "Directory for output files "
                             "(defaults to current directory)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ofs", "ofs",
                             "Output filename suffix "
                             "(default is \".ent\" for Seq-entry "
                             "or \".sqn\" for Seq-submit",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("stdout", "Write to stdout rather than files.  This does not work for Seq-submits. "
                      "Implies -no_asnval.");
    arg_desc->AddDefaultKey(
        "output-type", "ASN_OBJECT_TYPE",
        "This lets you force what kind of object is used for output. "
        " Forcing may cause some data to be thrown out (Example: "
        "if input is a Seq-submit and you force the output to be a "
        "Seq-entry, then the Seq-submit's data will be disregarded)",
        CArgDescriptions::eString,
        "AUTO" );
    arg_desc->SetConstraint("output-type",
        &(*new CArgAllow_Strings,
          "AUTO", "Seq-entry"));


    arg_desc->SetCurrentGroup("VALIDATION");

    arg_desc->AddOptionalKey("components", "components_file",
        "Bioseq-set of components, used for "
        "validation",
        CArgDescriptions::eInputFile);
    arg_desc->AddFlag("no_asnval",
                      "Do not validate using asnval");

    arg_desc->SetCurrentGroup("DESCRIPTORS");

    arg_desc->AddOptionalKey("dl", "definition_line",
        "Definition line (title descriptor)",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("nt", "tax_id",
                             "NCBI Taxonomy Database ID",
                             CArgDescriptions::eInteger);
    arg_desc->SetDependency("nt", CArgDescriptions::eExcludes, "chromosomes" );
    arg_desc->AddOptionalKey("on", "org_name",
                             "Organism name",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("on", CArgDescriptions::eExcludes, "chromosomes" );
    arg_desc->AddOptionalKey("sn", "strain_name",
                             "Strain name",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("sn", CArgDescriptions::eRequires, "on");
    arg_desc->SetDependency("sn", CArgDescriptions::eExcludes, "chromosomes" );

    ITERATE( TSimpleSubsourceMap, simple_src_it, sc_SimpleSubsourceMap ) {
        const string & sArgName = simple_src_it->first;
        const SSimpleSubsourceInfo & info = simple_src_it->second;
        arg_desc->AddOptionalKey(sArgName, info.m_pchSynopsis,
                             info.m_pchUsage,
                             CArgDescriptions::eString);
        arg_desc->SetDependency(sArgName, CArgDescriptions::eExcludes, "chromosomes" );
    }

    arg_desc->SetCurrentGroup("SEQ-IDS");

    arg_desc->AddFlag("fasta_id", "Parse object ids (col. 1) "
                      "as fasta-style ids if they contain '|'");
    arg_desc->AddDefaultKey("general_id", "general_db",
        "if set to non-empty string, local ids for object seq-ids will "
        "become general ids belonging to the given database",
        CArgDescriptions::eString, kEmptyStr );

    arg_desc->SetCurrentGroup("OTHER");

    arg_desc->AddFlag("fuzz100", "For gaps of length 100, "
                      "put an Int-fuzz = unk in the literal");
    
    arg_desc->AddOptionalKey("chromosomes", "chromosome_name_file",
                             "Mapping of col. 1 names to chromsome "
                             "names, for use as SubSource",
                             CArgDescriptions::eInputFile);
    arg_desc->AddFlag("gap-info",
                      "Set Seq-gap (gap type and linkage) in delta sequence");
    arg_desc->AddFlag("len-check",
        "Die if AGP's length does not match the length of the original template.");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static TTaxId s_GetTaxid(const COrg_ref& org_ref) {
    TTaxId taxid = ZERO_TAX_ID;
    int count = 0;
    ITERATE (COrg_ref::TDb, db_tag, org_ref.GetDb()) {
        if ((*db_tag)->GetDb() == "taxon") {
            count++;
            taxid = TAX_ID_FROM(CObject_id::TId, (*db_tag)->GetTag().GetId());
        }
    }
    if (count != 1) {
        throw runtime_error("found " + NStr::IntToString(count) + " taxids; "
                            "expected exactly one");
    }
    return taxid;
}


// Helper for removing old-name OrgMod's
struct SIsOldName
{
    bool operator()(CRef<COrgMod> mod)
    {
        return mod->GetSubtype() == COrgMod::eSubtype_old_name;
    }
};


/////////////////////////////////////////////////////////////////////////////
//  Run


int CAgpconvertApplication::Run(void)
{
    // Get arguments
    const CArgs & args = GetArgs();

    // load template file
    CRef<CSeq_entry> ent_templ;
    CRef<CSeq_submit> submit_templ;
    x_LoadTemplate( 
        args["template"].AsString(),
        ent_templ,
        submit_templ );

    // don't use any annots in the template, unless specifically requested
    if( ! args["keeptemplateannots"] ) {
        ent_templ->SetSeq().ResetAnnot();
    }

    // Deal with any descriptor info from command line
    if (args["dl"]) {
        const string& dl = args["dl"].AsString();
        ITERATE (CSeq_descr::Tdata, desc,
                 ent_templ->GetSeq().GetDescr().Get()) {
            if ((*desc)->IsTitle()) {
                throw runtime_error("-dl given but template contains a title");
            }
        }
        CRef<CSeqdesc> title_desc(new CSeqdesc);
        title_desc->SetTitle(dl);
        ent_templ->SetSeq().SetDescr().Set().push_back(title_desc);
    }
    if (args["nt"] || args["on"] || args["sn"] || 
        x_IsAnySimpleSubsourceArgSet() ) 
    {
        // consistency checks
        ITERATE (CSeq_descr::Tdata, desc,
                 ent_templ->GetSeq().GetDescr().Get()) {
            if ((*desc)->IsSource()) {
                throw runtime_error("BioSource specified on command line but "
                                    "template contains BioSource");
            }
        }

        // build a BioSource desc and add it to template
        CRef<CSeqdesc> source_desc(new CSeqdesc);

        // handle the simple subsource args, if any set
        ITERATE( TSimpleSubsourceMap, simple_src_it, sc_SimpleSubsourceMap ) {
            const string & sArgName = simple_src_it->first;
            const SSimpleSubsourceInfo & info = simple_src_it->second;
            if( args[sArgName] ) {
                CRef<CSubSource> sub_source( new CSubSource );
                sub_source->SetSubtype(info.m_eSubtype);
                sub_source->SetName(args[sArgName].AsString());
                source_desc->SetSource().SetSubtype().push_back(sub_source);
            }
        }

        // handle tax-related args, if any set
        x_HandleTaxArgs( source_desc );
        
        ent_templ->SetSeq().SetDescr().Set().push_back(source_desc);
    }

    CAgpConverter::TOutputFlags fAgpConvertOutputFlags = 0;
    if( args["fuzz100"] ) {
        fAgpConvertOutputFlags |= CAgpConverter::fOutputFlags_Fuzz100;
    }
    if( args["fasta_id"] ) {
        fAgpConvertOutputFlags |= CAgpConverter::fOutputFlags_FastaId;
    }
    if( args["gap-info"] ) {
        fAgpConvertOutputFlags |= CAgpConverter::fOutputFlags_SetGapInfo;
    }
    if( args["len-check"] ) {
        fAgpConvertOutputFlags |= CAgpConverter::fOutputFlags_AGPLenMustMatchOrig;
    }
    CAgpConverter agpConvert(
        CConstRef<CBioseq>( &ent_templ->GetSeq() ),
        ( submit_templ->IsEntrys() ? &submit_templ->GetSub() : NULL ),
        fAgpConvertOutputFlags,
        CRef<CAgpConverter::CErrorHandler>(m_pCustomErrorHandler.GetPointer()) );

    // add general_id transformer, if needed
    const string & sGeneralIdDb = args["general_id"].AsString();
    if( ! sGeneralIdDb.empty() ) {
        class CLocalToGeneralIdTransformer : 
            public CAgpConverter::IIdTransformer 
        {
        public:
            CLocalToGeneralIdTransformer(const string & sGeneralDb)
                : m_sGeneralDb(sGeneralDb) { }

            virtual bool Transform(CRef<objects::CSeq_id> pSeqId) const 
            {
                if( ! pSeqId || ! pSeqId->IsLocal() ) {
                    // only transform local ids
                    return false;
                }
                CRef<CSeq_id> pNewSeqId( new CSeq_id );
                CDbtag & dbtag = pNewSeqId->SetGeneral();
                dbtag.SetDb(m_sGeneralDb);
                if( pSeqId->GetLocal().IsId() ) {
                    dbtag.SetTag().SetId( pSeqId->GetLocal().GetId() );
                } else if( pSeqId->GetLocal().IsStr() ) {
                    dbtag.SetTag().SetStr( pSeqId->GetLocal().GetStr() );
                } else {
                    return false;
                }

                pSeqId->Assign( *pNewSeqId );
                return true;
            }

        private:
            string m_sGeneralDb;
        };
        CRef<CAgpConverter::IIdTransformer> pIdTransformer(
            new CLocalToGeneralIdTransformer(sGeneralIdDb) );

        agpConvert.SetIdTransformer( pIdTransformer.GetPointer() );
    }

    // if validating against a file containing
    // sequence components, load it and make
    // a mapping of ids to lengths
    if (args["components"]) {
        CRef<CBioseq_set> seq_set( new CBioseq_set );
        args["components"].AsInputFile() >> MSerial_AsnText >> *seq_set;
        agpConvert.SetComponentsBioseqSet(seq_set);
    }

    // if requested, load a file of mappings of
    // object identifiers to chromosome names
    if (args["chromosomes"]) {
        agpConvert.LoadChromosomeMap( args["chromosomes"].AsInputFile() );
    }

    // convert AGP file name args to strings
    vector<string> vecAgpFileNames;
    for( size_t idx = 1; idx <= args.GetNExtra(); ++idx ) {
        vecAgpFileNames.push_back( args[idx].AsString() );
        if( ! CFile(vecAgpFileNames.back()).Exists() ) {
            throw runtime_error( "AGP file not found: " + vecAgpFileNames.back() );
        }
    }

    if( args["stdout"] ) {
        CAgpConverter::TOutputBioseqsFlags fOutputBioseqsFlags = 
            CAgpConverter::fOutputBioseqsFlags_DoNOTUnwrapSingularBioseqSets;

        if( args["output-type"].AsString() == "Seq-entry" ) {
            fOutputBioseqsFlags |= CAgpConverter::fOutputBioseqsFlags_WrapInSeqEntry;
        }
        agpConvert.OutputBioseqs(
            cout,
            vecAgpFileNames,
            fOutputBioseqsFlags );
    } else {
        if( ! args["outdir"] ) {
            throw runtime_error("Please specify -stdout or -outdir");
        }

        CAsnvalRunner asnval_runner;
        agpConvert.OutputOneFileForEach(
            args["outdir"].AsString(),
            vecAgpFileNames,
            ( args["ofs"] ? args["ofs"].AsString() : kEmptyStr ),
            ( args["no_asnval"] ? NULL : &asnval_runner ) );
    }

    return 0;
}



void
CAgpconvertApplication::CAsnvalRunner::Notify(const string & file)
{
    // verify using asnval

    // command and args
    const char * pchCommand = "asnval";
    const char * asnval_argv[] = {
        pchCommand,
        "-Q", "2",
        "-o", "stdout", 
        "-i", file.c_str(), 
        NULL
    };

    // print what we're executing to cout
    string cmd = CExec::QuoteArg(pchCommand);
    for( size_t idx = 0; asnval_argv[idx]; ++idx ) {
        cmd += ' ';
        cmd += CExec::QuoteArg(asnval_argv[idx]);
    }
    cout << cmd << endl;

    // run asnval, and wait for it to finish
    CExec::SpawnVP(CExec::eWait, pchCommand, asnval_argv);
}

/////////////////////////////////////////////////////////////////////////////
//  x_LoadTemplate

void CAgpconvertApplication::x_LoadTemplate(
    const string & sTemplateLocation,
    CRef<CSeq_entry> & out_ent_templ,
    CRef<CSeq_submit> & out_submit_templ)
{
    const CArgs & args = GetArgs();

    out_ent_templ.Reset( new CSeq_entry );
    out_submit_templ.Reset( new CSeq_submit ); // possibly not used

    // check if the location doesn't exist, and see if we can
    // consider it some kind of sequence identifier
    if( ! CDirEntry(sTemplateLocation).IsFile() ) {
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

        out_ent_templ->Assign( *entry_h.GetCompleteSeq_entry() );
        return;
    }


    CNcbiIfstream istrm(sTemplateLocation.c_str());

    // guess format
    ESerialDataFormat eSerialDataFormat = eSerial_None;
    {{
        CFormatGuess::EFormat eFormat = 
            CFormatGuess::Format(istrm);

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
                "template file seems to be in an unsupported format: " 
                << CFormatGuess::GetFormatName(eFormat) );
            break;
        }

        istrm.seekg(0);
    }}

    unique_ptr<CObjectIStream> pObjIstrm( 
        CObjectIStream::Open(eSerialDataFormat, istrm, eNoOwnership) );

    // guess object type
    const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
        pObjIstrm->Read(ObjectInfo(*out_ent_templ), 
            CObjectIStream::eNoFileHeader);
    } else if( sType == CBioseq::GetTypeInfo()->GetName() ) {
        CRef<CBioseq> pBioseq( new CBioseq );
        pObjIstrm->Read(ObjectInfo(*pBioseq), 
            CObjectIStream::eNoFileHeader);
        out_ent_templ->SetSeq( *pBioseq );
    } else if( sType == CSeq_submit::GetTypeInfo()->GetName() ) {
        pObjIstrm->Read(ObjectInfo(*out_submit_templ),
            CObjectIStream::eNoFileHeader);
        if (!out_submit_templ->GetData().IsEntrys()
            || out_submit_templ->GetData().GetEntrys().size() != 1) 
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
        out_submit_templ->SetSub(*submit_block);
        CRef<CSeq_entry> ent(new CSeq_entry);
        CRef<CSeq_id> dummy_id(new CSeq_id("lcl|dummy_id"));
        ent->SetSeq().SetId().push_back(dummy_id);
        ent->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
        ent->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        out_submit_templ->SetData().SetEntrys().push_back(ent);
    } else {
        NCBI_USER_THROW_FMT("Template must be Seq-entry, Seq-submit, Bioseq or "
            "Submit-block.  Object seems to be of type: " << sType);
    }

    // for submit types, pull out the seq-entry inside and remember it
    if( out_submit_templ->IsEntrys() ) {
        out_ent_templ = out_submit_templ->SetData().SetEntrys().front();
    }

    // The template may contain a set rather than a seq.
    // That's OK if it contains only one na entry, which we'll use.
    if (out_ent_templ->IsSet()) {
        unsigned int num_nuc_ents = 0;
        CRef<CSeq_entry> tmp( new CSeq_entry );
        ITERATE (CBioseq_set::TSeq_set, ent_iter,
            out_ent_templ->GetSet().GetSeq_set()) {
                if ((*ent_iter)->GetSeq().GetInst().IsNa()) {
                    ++num_nuc_ents;
                    tmp->Assign(**ent_iter);
                    // Copy any descriptors from the set to the sequence
                    ITERATE (CBioseq_set::TDescr::Tdata, desc_iter,
                        out_ent_templ->GetSet().GetDescr().Get()) {
                            CRef<CSeqdesc> desc(new CSeqdesc);
                            desc->Assign(**desc_iter);
                            tmp->SetSeq().SetDescr().Set().push_back(desc);
                    }
                }
        }
        if (num_nuc_ents == 1) {
            out_ent_templ->Assign(*tmp);
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
            out_ent_templ->SetSeq().SetDescr().Set().push_back(desc);
        } catch (...) {
            break;
        }
    }

    if ( out_submit_templ->IsEntrys() ) {
        // Take Seq-submit.sub.cit and put it in the Bioseq
        CRef<CPub> pub(new CPub);
        pub->SetSub().Assign(out_submit_templ->GetSub().GetCit());
        CRef<CSeqdesc> pub_desc(new CSeqdesc);
        pub_desc->SetPub().SetPub().Set().push_back(pub);
        out_ent_templ->SetSeq().SetDescr().Set().push_back(pub_desc);
    }

    if( ! out_ent_templ->IsSeq() ) {
        throw runtime_error("The Seq-entry must be a Bioseq not a Bioseq-set.");
    }

    if( args["output-type"].AsString() == "Seq-entry" ) {
        // force Seq-entry by throwing out the Seq-submit
        out_submit_templ.Reset( new CSeq_submit );
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_IsAnySimpleSubsourceArgSet

bool CAgpconvertApplication::x_IsAnySimpleSubsourceArgSet(void)
{
    const CArgs & args = GetArgs();

    ITERATE( TSimpleSubsourceMap, simple_src_it, sc_SimpleSubsourceMap ) {
        const string & sArgName = simple_src_it->first;
        if( args[sArgName] ) {
            return true;
        }
    }

    // none are set
    return false;
}

/////////////////////////////////////////////////////////////////////////////
//  x_HandleTaxArgs

void CAgpconvertApplication::x_HandleTaxArgs( CRef<CSeqdesc> source_desc )
{
    const CArgs & args = GetArgs();

    // leave if no taxon-related arg is set
    if ( ! args["on"] && ! args["nt"] ) {
        return;
    }

    CTaxon1 cl;
    if (!cl.Init()) {
        throw runtime_error("failure contacting taxonomy server");
    }

    CConstRef<CTaxon2_data> on_result;
    CRef<CTaxon2_data> nt_result;
    CRef<COrg_ref> inp_orgref( new COrg_ref );

    if (args["on"]) {
        const string& inp_taxname = args["on"].AsString();
        inp_orgref->SetTaxname(inp_taxname);

        if (args["sn"]) {
            CRef<COrgMod> mod(new COrgMod(
                COrgMod::eSubtype_strain, args["sn"].AsString()) );
            inp_orgref->SetOrgname().SetMod().push_back(mod);
        }

        on_result = cl.LookupMerge(*inp_orgref);

        if (!on_result) {
            throw runtime_error("taxonomy server lookup failed");
        }
        if (!on_result->GetIs_species_level()) {
            throw runtime_error("supplied name is not species-level");
        }
        if (inp_orgref->GetTaxname() != inp_taxname) {
            cerr << "** Warning: taxname returned by server ("
                << on_result->GetOrg().GetTaxname()
                << ") differs from that supplied with -on ("
                << inp_taxname << ")" << endl;
            // an old-name OrgMod will have been added
            COrgName::TMod& mod = inp_orgref->SetOrgname().SetMod();
            mod.erase(remove_if(mod.begin(), mod.end(), SIsOldName()),
                mod.end());
            if (mod.empty()) {
                inp_orgref->SetOrgname().ResetMod();
            }
        }

        if (args["sn"]) {
            const string& inp_strain_name = args["sn"].AsString();
            vector<string> strain_names;
            ITERATE (COrgName::TMod, mod,
                inp_orgref->GetOrgname().GetMod()) 
            {
                    if ((*mod)->GetSubtype() == COrgMod::eSubtype_strain) {
                        strain_names.push_back((*mod)->GetSubname());
                    }
            }
            if (!(strain_names.size() == 1
                && strain_names[0] == inp_strain_name)) 
            {
                cerr << "** Warning: strain name " << inp_strain_name
                    << " provided but server lookup yielded ";
                if (strain_names.empty()) {
                    cerr << "no strain name" << endl;
                } else {
                    cerr << NStr::Join(strain_names, " and ") << endl;
                }
            }
        }
    }

    if (args["nt"]) {
        TTaxId inp_taxid = TAX_ID_FROM(int, args["nt"].AsInteger());
        nt_result = cl.GetById(inp_taxid);
        if (!nt_result->GetIs_species_level()) {
            throw runtime_error("taxid " + NStr::NumericToString(inp_taxid)
                + " is not species-level");
        }
        nt_result->SetOrg().ResetSyn();  // lose any synonyms
        TTaxId db_taxid = s_GetTaxid(nt_result->GetOrg());
        if (db_taxid != inp_taxid) {
            cerr << "** Warning: taxid returned by server ("
                << NStr::NumericToString(db_taxid)
                << ") differs from that supplied with -nt ("
                << inp_taxid << ")" << endl;
        }
        if (args["on"]) {
            TTaxId on_taxid = s_GetTaxid(on_result->GetOrg());
            if (on_taxid != db_taxid) {
                throw runtime_error("taxid from name lookup ("
                    + NStr::NumericToString(on_taxid)
                    + ") differs from that from "
                    + "taxid lookup ("
                    + NStr::NumericToString(db_taxid)
                    + ")");
            }
        }
    }

    if (args["on"]) {
        source_desc->SetSource().SetOrg().Assign(*inp_orgref);
    } else {
        source_desc->SetSource().SetOrg().Assign(nt_result->GetOrg());
    }
}

/////////////////////////////////////////////////////////////////////////////
// HandleError

void 
CAgpconvertApplication::CCustomErrorHandler::HandleError(
    CAgpConverter::EError eError, const string & sMessage ) const
{
    // assert that this function includes all possibilities
    _ASSERT( CAgpConverter::eError_END ==
        (CAgpConverter::eError_AGPLengthMismatchWithTemplateLength + 1) );

    switch(eError) {
    // these errors are instantly fatal
    case CAgpConverter::eError_OutputDirNotFoundOrNotADir:
    case CAgpConverter::eError_ComponentNotFound:
    case CAgpConverter::eError_ComponentTooShort:
    case CAgpConverter::eError_ChromosomeMapIgnoredBecauseChromosomeSubsourceAlreadyInTemplate:
    case CAgpConverter::eError_ChromosomeFileBadFormat:
    case CAgpConverter::eError_ChromosomeIsInconsistent:
    case CAgpConverter::eError_WrongNumberOfSourceDescs:
    case CAgpConverter::eError_EntrySkippedDueToFailedComponentValidation:
    case CAgpConverter::eError_EntrySkipped:
    case CAgpConverter::eError_AGPErrorCode:
    case CAgpConverter::eError_AGPLengthMismatchWithTemplateLength:
        NCBI_USER_THROW_FMT("ERROR: " << sMessage);
        break;

    // these errors are just written and we continue:
    case CAgpConverter::eError_SubmitBlockIgnoredWhenOneBigBioseqSet:
    case CAgpConverter::eError_SuggestUsingFastaIdOption:
    case CAgpConverter::eError_AGPMessage:
        cerr << sMessage << endl;
        break;
    default:
        _TROUBLE;
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAgpconvertApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAgpconvertApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
