#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>


BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

CVisualStudioProject * LoadFromXmlFile(const string& file_path)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_Xml, 
                                                    file_path, 
                                                    eSerial_StdWhenAny));

    auto_ptr<CVisualStudioProject> prj(new CVisualStudioProject());
    in->Read(prj.get(), prj->GetThisTypeInfo());
    return prj.release();
}


void SaveToXmlFile  (const string& file_path, 
                     const CVisualStudioProject& project)
{
    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir(dir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), ios::out | ios::trunc);
    if (!ofs) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
    }

    CObjectOStreamXml xs(ofs, false);
    xs.SetReferenceDTD(false);
    xs.SetEncoding(CObjectOStreamXml::eEncoding_Windows_1252);

    xs << project;
}

//---------------------------------------------------------------------------------------------------------------

class CGuidGenerator
{
public:
    ~CGuidGenerator(void);
    friend string GenerateSlnGUID(void);

private:
    CGuidGenerator(void);

    const string root_guid; // root GUID for MSVC solutions
    const string guid_base;
    string Generate12Chars(void);
    unsigned int m_Seed;

    string DoGenerateSlnGUID(void);
    set<string> m_Trace;
};


auto_ptr<CGuidGenerator> pGuidGen;


string GenerateSlnGUID(void)
{
    if (pGuidGen.get() == NULL) 
        pGuidGen = auto_ptr<CGuidGenerator>(new CGuidGenerator());

    return pGuidGen->DoGenerateSlnGUID();
}


CGuidGenerator::CGuidGenerator(void)
    :root_guid("8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"),
     guid_base("8BC9CEB8-8B4A-11D0-8D11-"),
     m_Seed(0)
{
}


CGuidGenerator::~CGuidGenerator(void)
{
}


string CGuidGenerator::Generate12Chars(void)
{
    CNcbiOstrstream ost;

    ost << hex << uppercase << noshowbase << setw(12) << setfill('A') 
        << m_Seed++ << ends << flush;

    return ost.str();
}


string CGuidGenerator::DoGenerateSlnGUID(void)
{
    for ( ;; ) {

        string proto = guid_base + Generate12Chars();
        if (proto != root_guid  &&  m_Trace.find(proto) == m_Trace.end()) {

            m_Trace.insert(proto);
            return "{" + proto + "}";
        }
    }
}


//------------------------------------------------------------------------------

string SourceFileExt(const string& file_path)
{
    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(file_path, &dir, &base, &ext);

    string source_file_prefix = CDirEntry::ConcatPath(dir, base);

    string file_path_cpp = source_file_prefix + ".cpp";
    if ( CFile(file_path_cpp).Exists() ) 
        return ".cpp";

    string file_path_c = source_file_prefix + ".c";
    if ( CFile(file_path_c).Exists() ) 
        return ".c";

    return "";
}


//------------------------------------------------------------------------------
END_NCBI_SCOPE
