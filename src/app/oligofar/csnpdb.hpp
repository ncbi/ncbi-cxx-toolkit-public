#ifndef OLIGOFAR_SNPDB__HPP
#define OLIGOFAR_SNPDB__HPP

#include "cseqcoding.hpp"

#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_cursor.hpp>

BEGIN_OLIGOFAR_SCOPES

class CSnpDbBase : public CBDB_File
{
public:
    enum ESeqIdType {
        eSeqId_integer,
        eSeqId_string
    };

    CSnpDbBase( ESeqIdType type = eSeqId_integer, const string& idprefix = "gi|", const string& idsuffix = "|" );

    string GetSeqId() const;
    
    int    GetPos() const { return m_pos; }
    float  GetProb( int i ) const { return m_prob[i]; }

    unsigned GetBase( double cutoff = 0.1 ) const { 
        return 
            (m_prob[0] > cutoff ? 0x01 : 0) |
            (m_prob[1] > cutoff ? 0x02 : 0) |
            (m_prob[2] > cutoff ? 0x04 : 0) |
            (m_prob[3] > cutoff ? 0x08 : 0) ;
    }

	ostream& Print( ostream& o ) const {
		 return o 
			 << GetSeqId() << "\t" 
			 << GetPos() << "\t"
			 << GetProb(0) << "\t"
			 << GetProb(1) << "\t"
			 << GetProb(2) << "\t"
			 << GetProb(3);
	}
protected:
    void ParseSeqId( const string& seqId );
    
protected:
    ESeqIdType m_seqIdType;
    string m_prefix;
    string m_suffix;

    CBDB_FieldString m_stringId;
    CBDB_FieldInt4   m_intId;
    CBDB_FieldInt4   m_pos;
    CBDB_FieldFloat  m_prob[4];
};

class CSnpDbCreator : public CSnpDbBase
{
public:
    enum EStrand {
        eStrand_forward,
        eStrand_reverse
    };
    
    CSnpDbCreator( ESeqIdType type = eSeqId_integer, const string& idprefix = "gi|", const string& idsuffix = "|" )
        : CSnpDbBase( type, idprefix, idsuffix ) {}

    bool Insert( const string& fullId, int pos, double a, double c, double g, double t, EStrand strand = eStrand_forward );
    bool Insert( const string& fullId, int pos, unsigned base = 0x0f, EStrand strand = eStrand_forward );
};

class CSnpDb : public CSnpDbBase
{
public:
    CSnpDb( ESeqIdType type = eSeqId_integer, const string& idprefix = "gi|", const string& idsuffix = "|" )
        : CSnpDbBase( type, idprefix, idsuffix ) {}
    
    ~CSnpDb() { Close(); }

    void Close() { m_cursor.reset(0); CSnpDbBase::Close(); }

    int  First( int seqId );
    int  First( const string& seqId );
    int  Next();
    
    bool Ok() const { return m_cursor.get() != 0; }

protected:
    auto_ptr<CBDB_FileCursor> m_cursor;
};

END_OLIGOFAR_SCOPES

#endif
    

