#ifndef OLIGOFAR_COLIGOFARCFG__HPP
#define OLIGOFAR_COLIGOFARCFG__HPP

#include "string-util.hpp"
#include <util/value_convert.hpp>
#include <set>
#include <getopt.h>

BEGIN_OLIGOFAR_SCOPES

class COligofarCfg
{
public:
    virtual ~COligofarCfg() {}
    enum EArg { eNoArg = 0, eArgRequired = 1, eArgOptional = 2 };
    enum EConfigSection { 
        eSection_ignore, 
        eSection_passopt,
        eSection_fileopt,
        eSection_hashscan,
        eSection_align,
        eSection_filter,
        eSection_other, 
        eSection_extended,
        eSection_END
    };
    enum ELongOpt { 
        kLongOptBase = 0x100,
        kLongOpt_pass0 = kLongOptBase + 0x00,
        kLongOpt_pass1 = kLongOptBase + 0x01,
        kLongOpt_min_block_length = kLongOptBase + 0x02,
        kLongOpt_NaHSO3 = kLongOptBase + 0x03,
        kLongOpt_maxInsertions = kLongOptBase + 0x04,
        kLongOpt_maxDeletions = kLongOptBase + 0x05,
        kLongOpt_maxInsertion = kLongOptBase + 0x07,
        kLongOpt_maxDeletion = kLongOptBase + 0x08,
        kLongOpt_addSplice = kLongOptBase + 0x09,
        kLongOpt_hashBitMask = kLongOptBase + 0x0a,
        kLongOpt_batchRange = kLongOptBase + 0x0b,
        kLongOpt_printStatistics = kLongOptBase + 0x0c,
        kLongOpt_writeConfig = kLongOptBase + 0x0d,
        kLongOpt_fastaParseIDs = kLongOptBase + 0x0e,
        kLongOptEnd
    };
    class COption
    {
    public:
        
        COption( int opt, const string& longopt, EArg arg, EConfigSection section, const string& help, const string& valhlp ) :
            m_opt( opt ), m_longopt( longopt ), m_arg( arg ), m_section( section ), m_help( help ), m_valhelp( valhlp ) {}
        int GetOpt() const { return m_opt; }
        int GetOptChar() const { return isascii( m_opt ) && isprint( m_opt ) ? char( m_opt ) : 0; }
        const char * GetName() const { return m_longopt.c_str(); }
        EArg GetArgRq() const { return m_arg; }
        EConfigSection GetSection() const { return m_section; }
        const string& GetHelp() const { return m_help; }
        const string& GetValHelp() const { return m_valhelp; }

    protected:
        int m_opt;
        string m_longopt;
        EArg m_arg;
        EConfigSection m_section;
        string m_help;
        string m_valhelp;
    };
    const char * GetOptString() const;
    const struct option * GetLongOptions() const;
    void AddOption( int opt, const string& longopt, EArg arg, EConfigSection section, const string& help, const string& valhlp = "" ) {
        if( m_optchar.count( opt ) || m_optchar.count( char( opt ) ) ) 
            THROW( logic_error, "Internal configuration error: duplicate option for long option " << longopt );
        if( m_longopt.count( longopt ) )
            THROW( logic_error, "Internal configuration error: duplicate option long option " << longopt );
        m_optchar.insert( opt ); m_optchar.insert( char( opt ) );
        m_longopt.insert( longopt );
        m_optionList.push_back( COption( opt, longopt, arg, section, help, valhlp ) ); 
    }
    void AddFlag( int opt, const string& longopt, EConfigSection section, const string& help ) {
        AddOption( opt, longopt, eNoArg, section, help );
    }
    void AddOption( int opt, const string& longopt, EConfigSection section, const string& help, const string& valhlp ) {
        AddOption( opt, longopt, eArgRequired, section, help, valhlp );
    }
    void InitArgs();
    void PrintBriefHelp( ostream& );
    void PrintBriefHelp( EConfigSection, ostream& );
    void PrintFullHelp( ostream& );
    void PrintFullHelp( EConfigSection, pair<int,int>& colw, ostream *,int pass );
    void PrintConfig( ostream& );
    void PrintConfig( EConfigSection, ostream&, int pass );
    // to be inherited in COligofarApp
    virtual void PrintArgValue( int opt, ostream& out, int pass ) = 0;
    virtual int  GetPassCount() const = 0;
protected:
    mutable string m_optstring;
    mutable vector<struct option> m_long_options;
    typedef list<COption> TOptionList;
    TOptionList m_optionList;
    set<int> m_optchar;
    set<string> m_longopt;
};

inline const char * COligofarCfg::GetOptString() const 
{
    m_optstring.clear();
    ITERATE( TOptionList, opt, m_optionList ) {
        if( char o = opt->GetOptChar() ) {
            m_optstring += o; 
            if( opt->GetArgRq() ) 
                m_optstring += ':';
            if( opt->GetArgRq() == eArgOptional )
                m_optstring += ':'; // second ':'
        }
    }
    return m_optstring.c_str();
}

inline const struct option * COligofarCfg::GetLongOptions() const 
{
    m_long_options.clear();
    m_long_options.resize( m_optionList.size() + 3 );
    int index = 0;
    ITERATE( TOptionList, opt, m_optionList ) {
        m_long_options[index].name = opt->GetName();
        m_long_options[index].has_arg = opt->GetArgRq();
        m_long_options[index].flag = 0;
        m_long_options[index].val = opt->GetOpt();
        ++index;
    }
    m_long_options[index].name = "pass0";
    m_long_options[index].has_arg = 0;
    m_long_options[index].flag = 0;
    m_long_options[index].val = kLongOpt_pass0;
    ++index;
    m_long_options[index].name = "pass1";
    m_long_options[index].has_arg = 0;
    m_long_options[index].flag = 0;
    m_long_options[index].val = kLongOpt_pass1;
    ++index;
    m_long_options[index].name = 0;
    m_long_options[index].has_arg = 0;
    m_long_options[index].flag = 0;
    m_long_options[index].val = 0;
    return &m_long_options[0];
}

END_OLIGOFAR_SCOPES

#endif
