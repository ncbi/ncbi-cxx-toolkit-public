#ifndef XUTILS_H
#define XUTILS_H

typedef std::list<ncbi::CRef<ncbi::objects::CSeq_loc> > TSeqLocList;
typedef std::list<ncbi::CRef<ncbi::objects::CDelta_seq> > TDeltaList;

void XGappedSeqLocsToDeltaSeqs(const TSeqLocList& locs, TDeltaList& deltas);
int XDateCheck(const ncbi::objects::CDate_std& date);

#endif // XUTILS_H
