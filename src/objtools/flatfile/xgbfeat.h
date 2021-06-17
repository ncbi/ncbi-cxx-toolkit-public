/*************************************************************************
* gbfeat.h:
*
*                                                              10-11-93
*************************************************************************/
#ifndef _GBFEAT_
#define _GBFEAT_

#include <objects/seqfeat/Gb_qual.hpp>
BEGIN_NCBI_SCOPE

typedef std::vector<CRef<objects::CGb_qual> > TQualVector; 

int XGBFeatKeyQualValid(objects::CSeqFeatData::ESubtype subtype, TQualVector& quals, bool error_msgs, bool perform_corrections);

/*   --  GB_ERR returns values */
END_NCBI_SCOPE

#define GB_FEAT_ERR_NONE 0
#define GB_FEAT_ERR_SILENT 1
#define GB_FEAT_ERR_REPAIRABLE 2
#define GB_FEAT_ERR_DROP 3 

#endif
