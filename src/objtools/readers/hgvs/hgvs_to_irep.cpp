#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiexpt.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/readers/hgvs/hgvs_parser.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CHgvsToIrepException : public CException
{
public:
    enum EErrCode {
        eParseError
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CHgvsToIrepException, CException);
};


const char* CHgvsToIrepException::GetErrCodeString(void) const
{
    switch( GetErrCode() ) {
        
        case eParseError:  return "eParserError";

        default: return CException::GetErrCodeString();
    }
};


class CHgvsToIrepConverter : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};

void CHgvsToIrepConverter::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("in",
         "input",
         "Text file containing hgvs expression",
         CArgDescriptions::eInputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("out",
         "output",
         "Output file for seqfeat; used in conjunction with -out",
         CArgDescriptions::eOutputFile,
         "-",
         CArgDescriptions::fPreOpen);

    // Program description
    string prog_description = "Convert hgvs expression to a Seq-feat\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
            prog_description, false);

    SetupArgDescriptions(arg_desc.release());

    CException::SetStackTraceLevel(eDiag_Warning);
    SetDiagPostLevel(eDiag_Info);
    return;
}

int CHgvsToIrepConverter::Run(void) 
{
    const CArgs& args = GetArgs();
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    CNcbiIstream& istr = args["in"].AsInputFile();
    CNcbiOstream& ostr = args["out"].AsOutputFile();

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
        NCBI_THROW(CHgvsToIrepException, eParseError, "Failed to parse " + hgvs_expression);
    }

    ostr << MSerial_AsnText << *irep;

    return 0;
}


int main(int argc, const char* argv[])
{
    return CHgvsToIrepConverter().AppMain(argc, argv);
}
