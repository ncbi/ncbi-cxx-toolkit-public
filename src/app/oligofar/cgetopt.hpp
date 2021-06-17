#ifndef CAPPARGS__HPP
#define CAPPARGS__HPP

#include <util/value_convert.hpp>
#include <corelib/ncbireg.hpp>
#include <getopt.h>
#include "string-util.hpp"
#include "debug.hpp"

BEGIN_NCBI_SCOPE

class COption;
class COptGroup;
class CGetOpt;

/* ========================================================================
 * Usage:
 *
 * bool boolFlag;
 * int  intFlags;
 * bool boolValue = true;
 * int  intValue = 10;
 * double dblValue = 12.5;
 * string stringValue = 14.6;
 * set<int> intSet;
 * list<string> strList;
 * pair<int,int> intRange;
 *
 * CGetOpt getopt( argc, argv );
 * getopt
 *  .AddGroup( "section", "description" )
 *  .AddVersion( "progname", "3.23", "linux" )
 *  .AddHelp( "command [options] file ...", "description" )
 *  .AddFlag( boolFlag, 'b', "bool-flag", "description" )
 *  .AddFlag( 0x01, intFlags, 'c', "int-flag-1", "description" )
 *  .AddFlag( 0x02, intFlags, 'd', "int-flag-2", "description" )
 *  .AddArg( boolValue, 'e', "bool-val", "description" )
 *  .AddArg( intValue, 'f', "int-val", "description" )
 *  .AddArg( dblValue, 'g', "double-val", "description" )
 *  .AddArg( intRange, 'i', "int-range", "description" )
 *  .AddSet( ",", intSet, 'k', "int-set", "description" ) // comma-separated 
 *  .AddList( strList,  'l', "str-list", "description" ) // no separator
 *  .Parse();
 *
 *  if( getopt.Done() ) return getopt.GetResultCode();
 *
 *  For ranges ':' is a separator.  Empty list value ( like -l '' ) clears list.
 */


class COption 
{
public:
    enum EValueMode { eNoValue = 0, eValueRequired = 1, eValueOptional = 2 };
    virtual ~COption() { delete m_next; }
    COption( EValueMode valueMode, char opt, const string& longOpt, const string& help ) :
        m_valueMode( valueMode ), m_option( opt ), m_longOption( longOpt ), m_helpText( help ), m_next( 0 ) {}
    COption& operator << ( COption& opt ) { if( m_next ) *m_next << opt; else m_next = &opt; return *this; }
    EValueMode GetValueMode() const { return m_valueMode; }
    char GetOption() const { return m_option; }
    const string& GetLongOption() const { return m_longOption; }
    const string& GetHelpText() const { return m_helpText; }
    const COption& GetNextOption() const { return *m_next; }
    bool IsLastOption() const { return !m_next; }
    virtual bool Handle( const char * arg ) = 0;
    virtual void PrintValue( ostream& ) const = 0;
protected:
    EValueMode m_valueMode;
    char m_option;
    string m_longOption;
    string m_helpText;
    COption * m_next;
};

class COptGroup
{
public:
    ~COptGroup() { delete m_first; delete m_next; }
    COptGroup( const string& name, const string& title = "" ) : 
        m_name( name ), m_title( title ), m_first( 0 ), m_next( 0 ) {}
    const string& GetName() const { return m_name; }
    const string& GetTitle() const { return m_title; }
    const COption& GetFirstOption() const { return *m_first; }
    const COptGroup& GetNextGroup() const { return *m_next; }
    bool HasOptions() const { return m_first; }
    bool IsLastGroup() const { return !m_next; }
    COptGroup& operator << ( COption& opt ) { if( m_first ) *m_first << opt; else m_first = &opt; return *this; }
    COptGroup& operator << ( COptGroup& opt ) { if( m_next ) *m_next << opt; else m_next = &opt; return *this; }
protected:
    string m_name;
    string m_title;
    COption * m_first;
    COptGroup * m_next;
};

template<class T>
class COptFlag : public COption
{
public:
    COptFlag( T& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eNoValue, opt, longOpt, help ), m_target( &tgt ), m_bits( ~T(0) ) { *m_target &= ~m_bits; }
    COptFlag( T bits, T& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eNoValue, opt, longOpt, help ), m_target( &tgt ), m_bits( bits ) { ASSERT( m_bits ); *m_target &= ~m_bits; }
    bool Handle( const char * ) { *m_target |= m_bits; return true; }
    void PrintValue( ostream& o ) const { o << ((*m_target & m_bits) ? "on" : "off"); }
protected:
    T * m_target;
    T   m_bits;
};

template<class T>
class COptFlags : public COption
{
public:
    typedef map<char,T> TFlags;
    COptFlags( const TFlags& flags, T& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ) { 
            copy( flags.begin(), flags.end(), inserter( m_flags, m_flags.end() ) ); 
        }
    bool Handle( const char * arg ) {
        char op = '=';
        switch( *arg ) { case '+': case '-': case '=': op = *arg++; break; }
        if( op == '=' ) { *m_target = T(0); op = '+'; }
        bool ok = true;
        for( ; *arg ; ++arg ) {
            typename TFlags::const_iterator i = m_flags.find( *arg );
            if( i == m_flags.end() ) {
                THROW( runtime_error, "Unknown flag " << *arg << " for " 
                     << (GetLongOption().length() ? "--" + GetLongOption(): ("-" + string(1,GetOption())) ) );
            } else {
                if( op == '+' ) *m_target |= i->second;
                else *m_target &= ~i->second;
            }
        }
        return ok;
    }
    void PrintValue( ostream& o ) const { 
        ITERATE( typename TFlags, f, m_flags ) {
            if( ( f->second & *m_target ) == f->second ) o << f->first;
        }
    }
protected:
    T * m_target;
    TFlags m_flags;
};

template<class T>
class COptChoice : public COption
{
public:
    typedef map<string,T> TChoice;
    COptChoice( const TChoice& choice, T& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ) { 
            copy( choice.begin(), choice.end(), inserter( m_choice, m_choice.end() ) ); 
        }
    bool Handle( const char * arg ) {
        typename TChoice::const_iterator i = m_choice.find( arg );
        if( i == m_choice.end() ) {
            THROW( runtime_error, "Unknown value " << arg << " for " 
                 << (GetLongOption().length() ? "--" + GetLongOption(): ("-" + string(1,GetOption())) ) );
        } else {
            *m_target = i->second;
        }
        return true;
    }
    void PrintValue( ostream& o ) const { 
        ITERATE( typename TChoice, f, m_choice ) {
            if( *m_target == f->second ) {
                o << f->first;
            }
        }
    }
protected:
    T * m_target;
    TChoice m_choice;
};

template<class T>
class COptArg : public COption
{
public:
    COptArg( T& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ), m_cnt(0) {}
    bool Handle( const char * arg ) { m_cnt++; *m_target = Convert( arg ); return true; }
    void PrintValue( ostream& o ) const { o << *m_target; }
protected:
    T * m_target;
    int m_cnt;
};

template<>
class COptArg<string> : public COption
{
public:
    COptArg( string& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ), m_cnt(0) {}
    bool Handle( const char * arg ) { m_cnt++; *m_target = arg; return true; }
    void PrintValue( ostream& o ) const { o << *m_target; }
protected:
    string * m_target;
    int m_cnt;
};

template<>
class COptArg<bool> : public COption
{
public:
    COptArg( bool& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ), m_cnt(0) {}
    bool Handle( const char * arg ) { 
        m_cnt++; 
        if( arg[0] && arg[1] == 0 ) {
            switch( tolower( *arg ) ) {
            case '+': case 'y': case 't': case '1': *m_target = true; return true;
            case '-': case 'n': case 'f': case '0': *m_target = false; return true;
            }                                        
        }
        *m_target = NStr::StringToBool( arg );
        return true;
    }
    void PrintValue( ostream& o ) const { o << NStr::BoolToString( *m_target ); }
protected:
    bool * m_target;
    int m_cnt;
};

template<>
class COptArg<unsigned> : public COption
{
public:
    COptArg( unsigned& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ), m_cnt(0) {}
    bool Handle( const char * arg ) { 
        m_cnt++; 
        char * oops = 0;
        if( arg[0] == '0' && arg[1] ) {
            if( arg[1] == 'x' || arg[1] == 'X' ) *m_target = strtoul( arg + 2, &oops, 16 );
            else if( arg[1] == 'b' || arg[1] == 'B' ) *m_target = strtoul( arg + 2, &oops, 2 );
            else *m_target = strtoul( arg + 1, 0, 8 );
        } else *m_target = strtoul( arg, &oops, 10 );
        if( oops == 0 || oops[0] != 0 ) THROW( runtime_error, "Failed to convert ``" << arg << "'' to unsigned int (" << arg[1] << ") (" << oops << ")" );
        return true;
    }
    void PrintValue( ostream& o ) const { o << NStr::BoolToString( *m_target ); }
protected:
    unsigned * m_target;
    int m_cnt;
};

template<>
template<class T>
class COptArg<pair<T,T> > : public COption
{
public:
    typedef pair<T,T> TPair;
    COptArg( TPair& tgt, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &tgt ), m_cnt(0) {}
    bool Handle( const char * arg ) {
        if( const char * colon = strchr( arg, ':' ) ) {
            m_target->first = Convert( string( arg, colon ) );
            m_target->second = Convert( colon + 1 );
            if( m_target->first > m_target->second ) swap( m_target->first, m_target->second );
        } else {
            m_target->first = m_target->second = Convert( arg );
        }
        return true;
    }
    void PrintValue( ostream& o ) const { o << m_target->first; if( m_target->second != m_target->first ) o << ":" << m_target->second; }
protected:
    TPair * m_target;
    int m_cnt;
};

template<class Set>
class COptArgSet : public COption
{
public:
    typedef Set TData;
    typedef typename TData::value_type value_type;
    COptArgSet( const string& delim, Set& dest, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &dest ), m_delim( delim ) {}
    COptArgSet( Set& dest, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &dest ) {}
    bool Handle( const char * arg ) { 
        if( arg == 0 || *arg == 0 ) { m_target->clear(); return true; }
        if( m_delim.length() == 0 ) m_target->insert( Convert<value_type>( arg ) ); 
        else Split( arg, m_delim, inserter( *m_target, m_target->end() ) );
        return true;
    }
    void PrintValue( ostream& o ) const {
        for( typename TData::const_iterator t = GetTarget().begin(), T = GetTarget().end(); t != T; ++t ) {
            char delim = m_delim.size() ? m_delim[0] : ',';
            if( t != m_target->begin() ) o << delim;
            o << *t;
        }
    }
    const TData& GetTarget() const { return *m_target; }
protected:
    Set * m_target;
    string m_delim;
};

template<class List>
class COptArgList : public COption
{
public:
    typedef List TData;
    typedef typename TData::value_type value_type;
    COptArgList( const string& delim, List& dest, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &dest ), m_delim( delim ) {}
    COptArgList( List& dest, char opt, const string& longOpt, const string& help ) :
        COption( eValueRequired, opt, longOpt, help ), m_target( &dest ) {}
    bool Handle( const char * arg ) { 
        if( arg == 0 || *arg == 0 ) { m_target->clear(); return true; }
        if( m_delim.length() == 0 ) m_target->push_back( Convert<value_type>( arg ) ); 
        else Split( arg, m_delim, back_inserter( *m_target ) );
        return true;
    }
    void PrintValue( ostream& o ) const {
        for( typename TData::const_iterator t = GetTarget().begin(), T = GetTarget().end(); t != T; ++t ) {
            char delim = m_delim.size() ? m_delim[0] : ',';
            if( t != m_target->begin() ) o << delim;
            o << *t;
        }
    }
    const TData& GetTarget() const { return *m_target; }
protected:
    List * m_target;
    string m_delim;
};

class COptVersion : public COption
{
public:
    COptVersion( CGetOpt& getOpt, const string& synopsis, const string& version, const string& build = "" ) :
        COption( eNoValue, 'V', "version", "Print version" ), m_getOpt( getOpt ), m_synopsis( synopsis ), m_version( version ), m_build( build ) {}
    bool Handle( const char * );
    void PrintValue( ostream& ) const {}
protected:
    CGetOpt& m_getOpt;
    string m_synopsis;
    string m_version;
    string m_build;
};

class COptHelp : public COption
{
public:
    COptHelp( CGetOpt& getOpt, const string& synopsis, const string& description = "" ) :
        COption( eNoValue, 'h', "help", "Print help" ), m_getOpt( getOpt ), m_synopsis( synopsis ), m_description( description ) {}
    bool Handle( const char * );
    void PrintValue( ostream& ) const {}
    string Quote( const string& s ) const;
    string ShortForm( const string& s ) const;
    string FormatLongOpt( const COption* option, const string& value ) const;
    string FormatShortOpt( const COption* option, const string& value ) const;
protected:
    CGetOpt& m_getOpt;
    string m_synopsis;
    string m_description;
};

class CIniPrint : public COption
{
public:
    CIniPrint( CGetOpt& getOpt ) :
        COption( eValueOptional, 'P', "print-config", "Print config file" ), 
        m_getOpt( getOpt ) {}
    bool Handle( const char * );
    void PrintValue( ostream& ) const {}
protected:
    CGetOpt& m_getOpt;
};

class CIniParser : public COption
{
public:
    CIniParser( CGetOpt& getOpt ) :
        COption( eValueRequired, 'C', "config", "Include config file" ), m_getOpt( getOpt ) {}
    bool Handle( const char * arg );
    void PrintValue( ostream& o ) const { 
        for( list<string>::const_iterator t = m_files.begin(), T = m_files.end(); t != T; ++t ) {
            if( t != m_files.begin() ) o << ";";
            o << *t;
        }
    }
protected:
    CGetOpt& m_getOpt;
    list<string> m_files;
};

class CGetOpt 
{
public:

    CGetOpt( int argc, char ** argv, const char * env = 0 ) : 
        m_firstGroup( 0 ), m_lastGroup( 0 ), 
        m_argIndex( 0 ), m_argc( argc ), m_argv( argv ), m_env( env ), 
        m_done( false ), m_result( 0 ) {}
    CGetOpt& operator << ( COptGroup& grp ) { if( m_lastGroup ) { *m_lastGroup << grp; m_lastGroup = &grp; } else m_firstGroup = m_lastGroup = &grp; return *this; }
    CGetOpt& operator << ( COption& opt ) { if( !m_firstGroup ) m_firstGroup = m_lastGroup = new COptGroup( "default" ); *m_lastGroup << opt; return *this; }

    enum EFlags { fPosix = 1, fNoPosix = 0, fDefault = fNoPosix };

    int Parse( int flags = fDefault );
    int ParseIni( IRegistry * reg );
    int ParseIni( const string& name );

    bool HasPositional() const { return m_argc - m_argIndex; }
    int GetArgIndex() const { return m_argIndex; }
    int GetArgCount() const { return m_argc; }
    int GetResultCode() const { return m_result; }
    bool Done() const { return m_done; }
    const char * GetArg( int i ) const { return m_argv[i]; }
    const COptGroup& GetFirstGroup() const { return *m_firstGroup; }

    void SetDone( int rc = 0 ) { m_result = rc; m_done = true; }

    CGetOpt& AddHelp( const string& synopsis, const string& description ) { return *this << *new COptHelp( *this, synopsis, description ); }
    CGetOpt& AddVersion( const string& synopsis, const string& version, const string& build = "" ) { return *this << *new COptVersion( *this, synopsis, version, build ); }
    CGetOpt& AddIniParser() { return *this << *new CIniParser( *this ); }
    CGetOpt& AddIniPrinter() { return *this << *new CIniPrint( *this ); }

    CGetOpt& AddGroup( const string& name, const string& title = "" ) { return *this << *new COptGroup( name, title ); }
    template<class T>
    CGetOpt& AddFlag( T& flag, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptFlag<T>( flag, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddFlag( T mask, T& flag, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptFlag<T>( mask, flag, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddFlags( const typename COptFlags<T>::TFlags& flagMap , T& flags, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptFlags<T>( flagMap, flags, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddChoice( const typename COptChoice<T>::TChoice& choiceMap, T& choice, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptChoice<T>( choiceMap, choice, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddArg( T& tgt, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptArg<T>( tgt, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddSet( T& tgt, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptArgSet<T>( tgt, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddSet( const string& delim, T& tgt, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptArgSet<T>( delim, tgt, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddList( T& tgt, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptArgList<T>( tgt, opt, longopt, help ); }
    template<class T>
    CGetOpt& AddList( const string& delim, T& tgt, char opt, const string& longopt = "", const string& help = "" ) { return *this << *new COptArgList<T>( delim, tgt, opt, longopt, help ); }
protected:
    COptGroup * m_firstGroup;
    COptGroup * m_lastGroup;
    int  m_argIndex;
    int  m_argc;
    char ** m_argv;
    const char * m_env;
    bool m_done;
    int  m_result;
};

inline int CGetOpt::ParseIni( const string& name ) 
{
    ifstream in( name == "-" ? "/dev/stdin" : name.c_str() ); 
    if( in.fail() ) THROW( runtime_error, "Failed to read file " + name + ": " + strerror( errno ) ); 
    CNcbiRegistry reg( in );
    return ParseIni( &reg );
}

inline int CGetOpt::ParseIni( IRegistry * reg ) 
{
    for( const COptGroup * grp = m_firstGroup; grp; grp = &(grp->GetNextGroup()) ) {
        if( reg->HasEntry( grp->GetName() ) ) {
            for( const COption * opt = &(grp->GetFirstOption()); opt; opt = &(opt->GetNextOption()) ) {
                if( opt->GetLongOption().length() ) {
                    if( reg->HasEntry( grp->GetName(), opt->GetLongOption() ) ) {
                        const_cast<COption*>( opt )->Handle( reg->Get( grp->GetName(), opt->GetLongOption() ).c_str() );
                    }
                }
            }
        }
    }
    return m_result;
}

inline int CGetOpt::Parse( int flags )
{
    m_done = false;
    m_argIndex = 0;
    m_result = 0;

    optind = 0;

    string options;
    if( (flags & (fPosix|fNoPosix)) == fPosix ) options += "+";
    vector<option> long_options;

    map<char, COption*> optMap;
    vector<COption*> longMap;

    for( const COptGroup * grp = m_firstGroup; grp; grp = &(grp->GetNextGroup() ) ) {
        for( const COption * opt = &( grp->GetFirstOption() ); opt; opt = &(opt->GetNextOption() ) ) {
            if( isprint( opt->GetOption() ) ) {
                options += opt->GetOption();
                switch( opt->GetValueMode() ) {
                case COption::eValueOptional: options += ':'; break;
                case COption::eValueRequired: options += ':'; break;
                default:;
                }
                if( optMap.find( opt->GetOption() ) != optMap.end() )
                    THROW( logic_error, "Option ``" << string( 1, opt->GetOption() ) << "'' is defined multiple times" );
                optMap.insert( make_pair( opt->GetOption(), const_cast<COption*>( opt ) ) );
            }
            if( int l = opt->GetLongOption().length() ) {
                struct option o;
                o.name = new char[l + 1];
                o.has_arg = opt->GetValueMode();
                o.flag = 0;
                o.val = opt->GetOption();
                strcpy( const_cast<char*>( o.name ), opt->GetLongOption().c_str() );
                longMap.push_back( const_cast<COption*>( opt ) );
                long_options.push_back( o );
            }
        }
    }
    long_options.push_back( option() );
    long_options.back().name = 0;
    long_options.back().has_arg = 0;
    long_options.back().flag = 0;
    long_options.back().val = 0;

    int optchar = 0;
    int index = -1;
    while( (optchar = getopt_long( m_argc, m_argv, options.c_str(), &long_options[0], &index ) ) != EOF ) {
        map<char,COption*>::const_iterator x = optMap.find( optchar );
        if( x != optMap.end() ) x->second->Handle( optarg );
        else if( index >= 0 && index < (int)long_options.size() ) 
            longMap[index]->Handle( optarg );
        else if( isprint( optopt ) ) 
            THROW( runtime_error, "Unhandled option ``-" << char(optopt) << "'' !" );
        else 
            THROW( runtime_error, "Unhandled option " << optarg << "!" );
    }

    m_argIndex = optind;

    for( vector<option>::iterator i = long_options.begin(); i != long_options.end(); ++i )
        delete [] i->name;

    return m_result;
}

inline bool CIniParser::Handle( const char * arg )
{
    m_files.push_back( arg ); 
    return m_getOpt.ParseIni( arg ) == 0; 
}

inline string COptHelp::ShortForm( const string& valstr ) const 
{
    if( valstr.length() > 23 ) 
        return valstr.substr( 0, 10 ) + "..." + valstr.substr( valstr.length() - 10 );
    return valstr;
}

inline bool COptVersion::Handle( const char * )
{
    m_getOpt.SetDone( 0 );
    cout << m_synopsis << " v." << m_version;
    if( m_build.length() ) cout << " (" << m_build << ")";
    cout << "\n";
    return true;
}

inline bool COptHelp::Handle( const char * )
{
    m_getOpt.SetDone( 0 );
    cout << m_synopsis << "\n"
        << m_description << "\n"
        << "where options are:\n";
    int ll = 0, ls = 0;
    for( const COptGroup* grp = &m_getOpt.GetFirstGroup(); grp; grp = &grp->GetNextGroup() ) {
        for( const COption* option = &grp->GetFirstOption(); option; option = &option->GetNextOption() ) {
            ostringstream val;
            option->PrintValue( val );
            string valstr = Quote( val.str() );
            ll = max( ll, (int)FormatLongOpt( option, valstr ).length() );
            ls = max( ls, (int)FormatShortOpt( option, valstr ).length() );
        }
    }
            
    for( const COptGroup* grp = &m_getOpt.GetFirstGroup(); grp; grp = &grp->GetNextGroup() ) {
        cout << "[" << grp->GetName() << "]";
        if( grp->GetTitle().length() ) cout << " - " << grp->GetTitle();
        cout << "\n";
        for( const COption* option = &grp->GetFirstOption(); option; option = &option->GetNextOption() ) {
            ostringstream val;
            option->PrintValue( val );
            string valstr = Quote( val.str() );

            cout 
                << "  " << setw( ll ) << left << FormatLongOpt( option, valstr )
                << "  " << setw( ls ) << left << FormatShortOpt( option, valstr )
                << "  " << option->GetHelpText();
            if( option->GetValueMode() == eNoValue && val.str().length() )
                cout << " [" << val.str() << "]";
            cout << "\n";
        }
    }
    return true;
}

inline bool CIniPrint::Handle( const char * fname )
{
    if( fname == 0 || (fname[0] == '-' && fname[1] == 0) ) fname = "/dev/stdout";
    ofstream o( fname );
    for( const COptGroup* grp = &m_getOpt.GetFirstGroup(); grp; grp = &grp->GetNextGroup() ) {
        o << ";;;; " << grp->GetTitle() << "\n";
        o << "[" << grp->GetName() << "]\n";
        for( const COption* option = &grp->GetFirstOption(); option; option = &option->GetNextOption() ) {
            if( option->GetLongOption().length() == 0 ) continue;
            if( option->GetValueMode() == eNoValue ) continue;
            o << ";; " << option->GetHelpText() << "\n";
            o << option->GetLongOption() << " = ";
            option->PrintValue( o );
            o << "\n";
        }
        o << "\n";
    }
    return true;
}

inline string COptHelp::FormatLongOpt( const COption* option, const string& valstr ) const
{
    if( option->GetLongOption().length() ) {
        string ret = "--" + option->GetLongOption();
        if( option->GetValueMode() != eNoValue ) {
            if( option->GetValueMode() == eValueOptional ) ret.append( "[" );
            ret.append( "=" );
            ret.append( valstr );
            if( option->GetValueMode() == eValueOptional ) ret.append( "]" );
        }
        return ret;
    } else {
        return "";
    }
}

inline string COptHelp::FormatShortOpt( const COption * option, const string& valstr ) const
{
    if( isprint( option->GetOption() ) ) {
        string ret = "-" + string( 1, option->GetOption() );
        if( option->GetValueMode() != eNoValue ) {
            ret.append( " " );
            ret.append( valstr );
        }
        return ret;
    } else {
        return "";
    }
}

inline string COptHelp::Quote( const string& str ) const
{
    if( str.length() == 0 ) return "''";
    if( str.find( '\'' ) != string::npos ) {
        string out = "\"";
        for( const char * c = str.c_str(); *c; ++c ) {
            switch( *c ) {
            case '\\':
            case '$':
            case '"': out.append( "\\" ); break;
            }
            out += *c;
        }
        return out + "\"";
    } else {
        bool need_quote = false;
        for( const char * c = str.c_str(); *c; ++c ) {
            if( isspace( *c ) || *c == '$' ) {
                need_quote = true;
            }
        }
        if( need_quote ) return "'" + str + "'";
        else return str;
    }
}



END_NCBI_SCOPE

#endif
