#ifndef FILE_CONTENTS_HEADER
#define FILE_CONTENTS_HEADER

#include <app/project_tree_builder/stl_msvc_usage.hpp>

#include <string>
#include <list>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

class CSimpleMakeFileContents
{
public:
    CSimpleMakeFileContents(void);
    CSimpleMakeFileContents(const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents& operator = (
	    const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents(const string& file_path);

    ~CSimpleMakeFileContents(void);

    /// Key-Value(s) pairs
    typedef map< string, list<string> > TContents;
    TContents m_Contents;

    static void LoadFrom(const string& path, CSimpleMakeFileContents * pFC);

    /// Debug dump
    void Dump(CNcbiOfstream& ostr) const;

private:
    void Clear(void);

    void SetFrom(const CSimpleMakeFileContents& contents);

    struct SParser;
    friend struct SParser;

    struct SParser
    {
        SParser(CSimpleMakeFileContents * pFC);

        void StartParse(void);
        void AcceptLine(const string& line);
        void EndParse(void);
        bool   m_Continue;
        pair<string, string> m_CurrentKV;

        CSimpleMakeFileContents * m_pFC;

    private:
        SParser();
        SParser(const SParser&);
        SParser& operator = (const SParser&);
    };

    void AddReadyKV(const pair<string, string>& kv);
};

END_NCBI_SCOPE
#endif //FILE_CONTENTS_HEADER