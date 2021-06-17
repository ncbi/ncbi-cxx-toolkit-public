#ifndef OLIGOFAR_FADICEAPP__HPP
#define OLIGOFAR_FADICEAPP__HPP

#include "capp.hpp"
#include <objects/seqloc/Seq_id.hpp>
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

// Main application class for read generator program
class CFaDiceApp : public CApp
{
public:
    typedef objects::CBioseq::TId TIds;
    CFaDiceApp( int argc, char ** argv );
protected:
    virtual void Help( const char * );
    virtual void Version( const char * );
    virtual int  Execute();

    virtual const option * GetLongOptions() const;
    virtual const char * GetOptString() const;
    virtual int ParseArg( int, const char *, int );
protected:
    void Process( istream&, ostream& );
    void Process( const string& seqid, string& sequence, ostream& );
    void GeneratePair( const string& seqid, const string& sequence, int, ostream& );
    void GenerateRead( const string& seqid, const string& sequence, int, ostream& );
    void ModifyRead( string& read, int& cnt, int& gcnt );
    void UpdateReadStrand( string& read, bool& revcompl, int s );
    void RecodeRead( string& read ) const;
    void FormatRead( string& read ) const;
    bool DiceStrand( int s ) const;

    int m_strands;
    int m_readlen;
    int m_pairlen;
    int m_margin;
    int m_pairStrands;
    bool m_colorspace;
    double m_prbMismatch;
    double m_prbIndel;
    double m_coverage;
    double m_qualityDegradation;
    string m_output;
};

END_OLIGOFAR_SCOPES

#endif
