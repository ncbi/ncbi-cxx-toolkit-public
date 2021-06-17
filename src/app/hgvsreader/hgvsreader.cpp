#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/hgvs/protein_irep_to_seqfeat.hpp>
#include <objtools/readers/hgvs/na_irep_to_seqfeat.hpp>
//#include <objtools/readers/hgvs/hgvs_validator.hpp>
#include <objtools/readers/hgvs/hgvs_parser.hpp>
#include <objtools/readers/hgvs/post_process.hpp>
#include <corelib/ncbiexpt.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


class CHgvsVariantException : public CException
{
public:
    enum EErrCode {
        eParseError
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CHgvsVariantException, CException);
};


const char* CHgvsVariantException::GetErrCodeString(void) const 
{
    switch( GetErrCode() ) {
    case eParseError:  return "eParseError";
    default:           return CException::GetErrCodeString();
    }
}


class CHgvsToSeqfeatConverter : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};

void CHgvsToSeqfeatConverter::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("i",
         "input",
         "Text file containing hgvs expression",
         CArgDescriptions::eInputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("o",
         "output",
         "Output file for seqfeat; used in conjunction with -out",
         CArgDescriptions::eOutputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddFlag("return-irep", 
                      "Return intermediate representation as output");

    // Program description
    string prog_description = "Convert hgvs expression to a Seq-feat\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
            prog_description, false);

    SetupArgDescriptions(arg_desc.release());

    CException::SetStackTraceLevel(eDiag_Fatal);
    SetDiagPostLevel(eDiag_Info);
    return;
}


int CHgvsToSeqfeatConverter::Run(void) 
{
    const CArgs& args = GetArgs();
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    string hgvs_expression;
    try {
        istr >> hgvs_expression;
    } catch (CEofException e) {
        return 1;
    }

    CHgvsParser hgvs_parser;
    CRef<CVariantExpression> irep;
    bool success = hgvs_parser.Apply(hgvs_expression, irep);
   
    if (!success) {
        NCBI_THROW(CHgvsVariantException, eParseError, "Failed to parse " + hgvs_expression);
    }

    if (args["return-irep"]) {
        ostr << MSerial_AsnText << *irep;
        return 0;
    }

    CRef<CSeq_feat> seq_feat;
    auto seq_type = irep->GetSequence_variant().GetSeqtype();
    if (seq_type == eVariantSeqType_p) {
        unique_ptr<CHgvsIrepReader> irep_reader(new CHgvsProtIrepReader(*scope));
        seq_feat = irep_reader->CreateSeqfeat(*irep);
    } else {
        unique_ptr<CHgvsIrepReader> irep_reader(new CHgvsNaIrepReader(*scope));
        seq_feat = irep_reader->CreateSeqfeat(*irep);

    }
    
    bool IsCDS = (seq_type == eVariantSeqType_c);

    g_ValidateVariationSeqfeat(*seq_feat, scope, IsCDS);

    ostr << MSerial_AsnText << *seq_feat;
    return 0;
}


int main(int argc, const char* argv[])
{
    SetDiagFilter(eDiagFilter_All, "!(1306.12)");
    return CHgvsToSeqfeatConverter().AppMain(argc, argv);
}
