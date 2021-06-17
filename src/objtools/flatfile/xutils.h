#ifndef XUTILS_H
#define XUTILS_H
BEGIN_NCBI_SCOPE

typedef std::list<CRef<objects::CSeq_loc> > TSeqLocList;
typedef std::list<CRef<objects::CDelta_seq> > TDeltaList;

void XGappedSeqLocsToDeltaSeqs(const TSeqLocList& locs, TDeltaList& deltas);
int XDateCheck(const objects::CDate_std& date);

END_NCBI_SCOPE

#endif // XUTILS_H
