/*************************************************************************
* xgbparint.h:
*
*                                                              06-04-2016
*************************************************************************/
#ifndef XGBPARINT_H
#define XGBPARINT_H

extern const Char* seqlitdbtag;
extern const Char* unkseqlitdbtag;

typedef std::list<ncbi::CRef<ncbi::objects::CSeq_id> > TSeqIdList;
typedef std::list<ncbi::CRef<ncbi::objects::CSeq_loc> > TSeqLocList;

typedef void(*X_gbparse_errfunc) (const Char*, const Char*);
typedef Int4(*X_gbparse_rangefunc) (void*, const ncbi::objects::CSeq_id& id);

void xinstall_gbparse_error_handler (X_gbparse_errfunc new_func);
void xinstall_gbparse_range_func (void* data, X_gbparse_rangefunc new_func);

ncbi::CRef<ncbi::objects::CSeq_loc> xgbparseint_ver (char* raw_intervals, bool& keep_rawPt, bool& sitesPt, int& num_errsPt,
                                                     const TSeqIdList& seq_ids, bool accver);

#endif // XGBPARINT_H
