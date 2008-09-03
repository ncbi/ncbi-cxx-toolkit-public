#ifndef OLIGOFAR_IALIGNER__HPP
#define OLIGOFAR_IALIGNER__HPP

#include "talignerbase.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner 
{
public:
	IAligner() : m_bestQueryScore(0) {}
	virtual ~IAligner() {}
	// qlen, slen should be negative for reverse strand
	virtual void Align( CSeqCoding::ECoding qenc, const char * qseq, int qlen, 
						CSeqCoding::ECoding send, const char * sseq, int slen, int flags ) = 0; 
	const CAlignerBase& GetAlignerBase() const { return m_alignerBase; }
	void SetScoreTbl( const CScoreTbl& scoreTbl ) { m_alignerBase.SetScoreTbl( scoreTbl ); }
	void SetBestPossibleQueryScore( double s ) { m_bestQueryScore = s; }
	double GetBestPossibleQueryScore() const { return m_bestQueryScore; }
protected:
	CAlignerBase m_alignerBase;
	double m_bestQueryScore;
};

END_OLIGOFAR_SCOPES

#endif
