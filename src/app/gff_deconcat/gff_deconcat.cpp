#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/line_reader.hpp>


BEGIN_NCBI_SCOPE


class CGffDeconcatApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    using TIdmap = map<string, list<string>>;

private:
    void xProcessFile(CNcbiIstream& istr);
    void xReadFile(CNcbiIstream& istr, TIdmap& id_map);
    void xProcessLine(const string& line, TIdmap& id_map);
    void xWriteFile(CNcbiOstream& ostr,
                    const string& header, 
                    const list<string>& body);
};


void CGffDeconcatApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());


    arg_desc->AddKey("i", "InputFile",
                     "GFF input filename", 
                     CArgDescriptions::eInputFile);

    SetupArgDescriptions(arg_desc.release());
}


int CGffDeconcatApp::Run(void)
{
    const CArgs& args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    xProcessFile(istr);

    return 0;
}


void CGffDeconcatApp::xProcessFile(CNcbiIstream& istr) {

    TIdmap id_map;
    xReadFile(istr, id_map);

    return;
}


void CGffDeconcatApp::xProcessLine(const string& line, TIdmap& id_map) {
    vector<string> columns;
    NStr::Split(line, " \t", columns, NStr::eMergeDelims);
    if (columns.size() <= 1) {
        return;
    }    
    id_map[columns[0]].push_back(line);
}


void CGffDeconcatApp::xReadFile(CNcbiIstream& istr, TIdmap& id_map)
{
    id_map.clear();
    CStreamLineReader lr(istr);

    string line;
    while ( !lr.AtEOF() ) {
        line = *++lr;
        if (NStr::IsBlank(line) || 
            line[0] == '#') {
            continue;
        }
        xProcessLine(line, id_map);
    }
    return;
}

void CGffDeconcatApp::xWriteFile(CNcbiOstream& ostr, 
                                 const string& header, 
                                 const list<string>& body) 
{
    return;
}



END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CGffDeconcatApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
