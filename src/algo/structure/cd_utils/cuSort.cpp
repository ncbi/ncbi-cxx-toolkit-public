#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSort.hpp>

#define _SWAP(a,b) temp=(a);(a)=(b);(b)=temp;
#define _vsortCBQuickM 7

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

int algIntSORTFunction(void * pVal,int i, int j)
{
	int * iVal=(int * ) pVal;
	if (iVal[i]>iVal[j])return 1;
	else if (iVal[i]<iVal[j])return -1;
	else return 0;
}


void   algSortQuickCallbackIndex(void * pVal,int n,int * istack,int * ind,algSORTFunction isCondFunc)
{
    int   i,ir=n-1,j,k,l=0;
    int   jstack=0;
    int   inda,temp;
    //int *  rrd,* ord;

    
    /* initilize straight order */
    for (i=l;i<=ir;i++)ind[i]=i;

    for (;;) { /*Insertion sort when subarray small enough. */
        if (ir-l < _vsortCBQuickM) {
            for (j=l+1;j<=ir;j++) {                
                inda=ind[j];
                for (i=j-1;i>=l;i--) {
                    if(isCondFunc(pVal,ind[i],inda)<=0)break;
                    ind[i+1]=ind[i];
                }
                ind[i+1]=inda;
            }
            if (jstack == 0) break;
            ir=istack[jstack--]; /* Pop stack and begin a new round of partitioning.  */
            l=istack[jstack--];
        } else {

            k=(l+ir) >> 1; /* Choose median of left, center, and right elements as partitioning element a. Alsorearrange so that a[l]<=a[l+1]<=a[ir]. */
            _SWAP(ind[k],ind[l+1])
            if(isCondFunc(pVal,ind[l],ind[ir])>0){_SWAP(ind[l],ind[ir])}
            if(isCondFunc(pVal,ind[l+1],ind[ir])>0){_SWAP(ind[l+1],ind[ir])}
            if(isCondFunc(pVal,ind[l],ind[l+1])>0){_SWAP(ind[l],ind[l+1])}
            
            i=l+1; /* Initialize pointers for partitioning. */
            j=ir; 
            inda=ind[l+1];
            for (;;) { /* Beginning of innermost loop. */
                do i++; while (isCondFunc(pVal,ind[i],inda)<0); /* Scan up to nd element > a. */
                do j--; while (isCondFunc(pVal,ind[j],inda)>0); /* Scan up to nd element > a. */
                if (j < i) break; /* Pointers crossed. Partitioning complete. */
                _SWAP(ind[i],ind[j]); /* Exchange elements. */
            } /* End of innermost loop. */
            ind[l+1]=ind[j];  /* Insert partitioning element. */
            ind[j]=inda;
            jstack += 2;
                /* Push pointers to larger subarray on stack, process smaller subarray immediately. */
            if (ir-i+1 >= j-l) {
                istack[jstack]=ir;
                istack[jstack-1]=i;
                ir=j-1;
            } else {
                istack[jstack]=j-1;
                istack[jstack-1]=l;
                l=i;
            }
        }
    }
    
    /* remember order */
    /*ord=ind;
    if(toDO&vSORTNUMBER){ord+=n;for(i=0;i<n;i++)ord[ind[i]]=i;}
    if(toDO&vSORTORDER){
        ord+=n;rrd=ord+n;
        rrd[ind[0]]=-1;
        ord[ind[0]]=ind[1];
        for(i=1;i<n-1;i++){
            rrd[ind[i]]=ind[i-1];
            ord[ind[i]]=ind[i+1];
        }
        rrd[ind[n-1]]=ind[n-2];
        ord[ind[n-1]]=-1;
    }*/
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE



