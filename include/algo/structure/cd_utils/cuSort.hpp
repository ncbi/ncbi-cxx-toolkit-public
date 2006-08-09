
#ifndef CU_SORT_HPP
#define CU_SORT_HPP

#include    <math.h>
#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

typedef int (* algSORTFunction)(void * pVal,int i, int j);

// todo value is a flag of 
//#define algSORTNUMBER     0x01
//#define algSORTORDER      0x02

// istack  - is temproary buffer of lenght n
// ind -  are the sorted indexes. 
 

NCBI_CDUTILS_EXPORT 
void   algSortQuickCallbackIndex(void * pVal,int n,int * istack,int * ind,algSORTFunction isCondFunc);
NCBI_CDUTILS_EXPORT 
int algIntSORTFunction(void * pVal,int i, int j);

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif

