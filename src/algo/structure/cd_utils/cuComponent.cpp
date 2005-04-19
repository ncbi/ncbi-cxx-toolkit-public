#include <ncbi_pch.hpp>

#include <vector>
#include <string>
#include <list>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuComponent.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

#define SEQ_NAME_MANGLING_ON
#define SEQ_STORE_IN_CHILD
// in EraseSequences check if SeqEntry is used by component parent relations

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/
_/ internal functions 
_/
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
static CDomain_parent::EParent_type defParTypes[]={
	CDomain_parent::eParent_type_classical,CDomain_parent::eParent_type_fusion,CDomain_parent::eParent_type_deletion,
    CDomain_parent::eParent_type_permutation,CDomain_parent::eParent_type_other};

// resets if nothing to be hold 
// fixes parent type : single non-deletion parent is always classical and multiple - always fusion
static void fixParentType(CCdCore * pCD)
{
	if(!pCD->IsSetAncestors())
		return ;
	list< CRef< CDomain_parent > > & fus=pCD->SetAncestors();
	if(fus.size()==0){
		pCD->ResetAncestors();
		return;
	}

	CDomain_parent::EParent_type needToBeType= (fus.size()>1) ? CDomain_parent::eParent_type_fusion :CDomain_parent::eParent_type_classical;

	list< CRef< CDomain_parent > > ::iterator iDom;
	//	if there were a classical parents reassign those into fusion parents
	for(iDom=fus.begin() ; iDom!=fus.end() ; iDom++) {
		CRef< CDomain_parent > &dom=(*iDom);
		if(dom->GetParent_type()==CDomain_parent::eParent_type_deletion)
			continue;
		if(dom->GetParent_type()!=needToBeType)
			dom->SetParent_type(needToBeType);
	}
}



// removes a component parent relation 
static bool removeComplexParent(CCd * pCD, CCdd_id * cdid)
{
    // if there is no complex parent
    if(!pCD->IsSetAncestors())
		return false;

    // find that domain parent and erase it 
	list< CRef< CDomain_parent > > & fus=pCD->SetAncestors();
	list< CRef< CDomain_parent > > ::iterator iDom;
	for(iDom=fus.begin() ; iDom!=fus.end() ; ) {
		CRef< CDomain_parent > &dom=(*iDom);
		if( dom->GetParentid().GetGid().GetAccession()==cdid->GetGid().GetAccession() ){
			fus.erase(iDom);
			iDom=fus.begin(); // to start Again deleting ... iterators are invalid after erase 
			continue;
		}
		iDom++;
	}


    return  true;
}


// squizes pGuideDD so it doesn't overlap with other component parents 
// and obeys pCD's block structure
static int fixMapComplexParentDDs(CCdCore * pCD, TDendiag * pGuideDD,string & err)
{
	TDendiag DDAtStage[2],srcDD;
	TDendiag * pGd=pGuideDD, *pIn,InDD,GdDD;
	int cnt,i;
	

    if(!pCD->IsSetAncestors())
        return pGuideDD->size();

    //list< CRef< CDomain_parent > > & fus=pCD->SetAncestors().SetComplexparents().SetFusionparents();
	list< CRef< CDomain_parent > > & fus=pCD->SetAncestors();
    list< CRef< CDomain_parent > >::iterator dom;
    for(i=0,dom=fus.begin(); dom!=fus.end(); dom++,i++){
		CRef<CSeq_align> pAl= *((*dom)->SetSeqannot().SetData().SetAlign().begin());
		pIn=&pAl->SetSegs().SetDendiag(); 
		//pOut=&DDAtStage[i%2];pOut->clear();
		DDAtStage[0].clear();
		InDD.clear();
		GdDD.clear();
		ddRecompose(pIn,1,0,&InDD);
		ddRecompose(pGd,1,0,&GdDD);
				string debug1=ddAlignInfo(&InDD);
				string debug2=ddAlignInfo(&GdDD);
			
		ddDifferenceResidues(&InDD,&GdDD,&DDAtStage[0]);
		if(!DDAtStage[0].size()){pGd=&DDAtStage[0];break;}
				string debug3=ddAlignInfo(&DDAtStage[0]); 
		DDAtStage[1].clear();
		cnt=ddRemap(&GdDD,1,&DDAtStage[0],0,&DDAtStage[1],2,1,DD_NOFLAG,err);
				string debug4=ddAlignInfo(&DDAtStage[1]); 
		if(!cnt){pGd=&DDAtStage[1];break;}
				
		pGd=&DDAtStage[1];
    }

	TDendiag_cit pDD;
	pGuideDD->clear();
	for(pDD=pGd->begin(); pDD!=pGd->end(); pDD++)pGuideDD->push_back(*pDD); 

	return cnt;

}
// adds a complex parent of a given type with given Cdd_id, DensDiags and SeqEntry
static bool addComplexParent(CCdCore * pCD, CCdd_id * cdid, TDendiag * pGuideDD,CRef< CSeq_entry > & srcSeq, int parType)
{
	string err;
	
    // prepare new Seq
	CRef<CSeq_entry > pSrcSeq;
	pSrcSeq.Reset(CopyASNObject(*srcSeq, &err));
	
    // prepare DDs     
	CRef<CSeq_align> pNewAlign ( new CSeq_align);
    pNewAlign->SetType(CSeq_align_Base::eType_diags);
    pNewAlign->SetDim(2);
    for(TDendiag_cit pDD=pGuideDD->begin(); pDD!=pGuideDD->end(); pDD++) 
		pNewAlign->SetSegs().SetDendiag().push_back(*pDD); 

    // insert the parent
    CRef< CDomain_parent > dom(new CDomain_parent);
    dom->SetParentid(*cdid);
    if (pGuideDD->size()) 
        dom->SetSeqannot().SetData().SetAlign().push_back(pNewAlign);
    else 
        dom->ResetSeqannot();
	dom->SetParent_type(defParTypes[parType-PARENT_CLASSIC]);
	list< CRef< CDomain_parent > > & fus=pCD->SetAncestors();
	fus.push_back(dom);

    return  true;
}




/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/
_/ external functions
_/
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
bool obeysParentTypeConstraints(const CCdCore* pCD) {
    bool isAncestors;
    bool result = false;
    int  nAncestors, nClassical = 0;

    if (pCD) {
        //  Check that one or neither of 'ancestors' or 'parent' is set.
        //  If the later is set, automatically means a single classical parent.
        isAncestors = pCD->IsSetAncestors();
        result = (isAncestors) ? !pCD->IsSetParent() : true;
        if (result && isAncestors) {
            list< CRef< CDomain_parent > >::const_iterator pit = pCD->GetAncestors().begin(), pit_end = pCD->GetAncestors().end();
            while (pit != pit_end) {
                if ((*pit)->GetParent_type() == CDomain_parent::eParent_type_classical) {
                    ++nClassical;
                }
                ++pit;
            }
            nAncestors = pCD->GetAncestors().size();
            result = ((nClassical == 1 && nAncestors == 1) ||
                      (nClassical == 0 && nAncestors  > 0));
        }
    }
    return result;
}


int getNumRepeats(const CCdCore* pParent, const CCdCore* pChild, const CDomain_parent::EParent_type* parentType) {
    int numRepeats = 0;
    int numParents = 0;
    vector<CDomain_parent::EParent_type> parentTypes;
    vector<CCdCore*> parents;

    if (parentType) {
        numParents = getParentTypes(pChild, parentTypes);
    }

    numParents = getComponentParents(pChild, NULL, &parents, true);
    for (int i = 0; i < numParents; ++i) {
        if (parents[i] == pParent) {
            if (parentType == NULL || parentTypes[i] == *parentType) {
                ++numRepeats;
            }
        }
    }

    return numRepeats;
}

int getRepeatNumberForGuideIndex(const CCdCore* pChild, int guideAlignmentIndex, const CDomain_parent::EParent_type* parentType) {
    int repeatNumber = 0;
    int numParents = 0;
    CCdCore* impliedParent = NULL;
    vector<CDomain_parent::EParent_type> parentTypes;
    vector<CCdCore*> parents;

    if (parentType) {
        numParents = getParentTypes(pChild, parentTypes);
    }

    numParents = getComponentParents(pChild, NULL, &parents, true);
    if (guideAlignmentIndex >=0 && guideAlignmentIndex < numParents) {
        impliedParent = parents[guideAlignmentIndex];
        for (int i = 0; i <= guideAlignmentIndex; ++i) {
            if (parents[i] == impliedParent) {
                if (parentType == NULL || parentTypes[i] == *parentType) {
                    ++repeatNumber;
                }
            }
        }
    }
    return repeatNumber;
}

//  If nThRepeat > 0, return the type of the nThRepeat of the parent in the child's list of
//  parents, where nThRepeat == 1 is the first (equivalent to the default), 2 is the second, etc.
//  If nThRepeat > # occurrences, return 'eParent_type_other' to signal no such parent.
CDomain_parent::EParent_type getParentType(const CCdCore* pParent, const CCdCore* pChild, int nThRepeat) {

    CDomain_parent::EParent_type parentType = CDomain_parent::eParent_type_other;
    CDomain_parent* domainParent = NULL;
    int nParents = 0, ctr = 0;

    if (pParent && pChild && nThRepeat >= 0) {
        if (pChild->IsSetParent() && nThRepeat <= 1) {  //  support deprecated 'parent' field in spec
            parentType = CDomain_parent::eParent_type_classical;
        } else if (pChild->IsSetAncestors()) {
            vector< CCdCore* > parentCds;
            vector<CDomain_parent::EParent_type> parentTypes;

            //  The default value of zero means return the type of first repeated parent instance found.
            if (nThRepeat == 0) {
                nThRepeat++;
            }

            nParents = getComponentParents(pChild, NULL, &parentCds, true);
            if (nParents >= nThRepeat && nParents == getParentTypes(pChild, parentTypes)) {
                for (int i = 0; i < nParents; ++i) {
                    if (parentCds[i] == pParent) {
                        ++ctr;
                        if (ctr == nThRepeat) {
                            parentType = parentTypes[i];
                            break;
                        }
                    }
                }
            }
        }
    }

    return parentType;
}

CDomain_parent::EParent_type getParentTypeByIndex(const CCdCore* pChild, int guideAlignmentIndex) {
    CDomain_parent::EParent_type parentType = CDomain_parent::eParent_type_other;
    vector<CDomain_parent::EParent_type> parentTypes;
    int nParents = getParentTypes(pChild, parentTypes);

    if (guideAlignmentIndex >=0 && guideAlignmentIndex < nParents) {
        parentType = parentTypes[guideAlignmentIndex];
    }
    return parentType;
}

int getParentTypes(const CCdCore* pChild, vector<CDomain_parent::EParent_type>& parentTypes) {

    parentTypes.clear();
    if (pChild) {
        if (pChild->IsSetParent()) {  //  support deprecated 'parent' field in spec
            parentTypes.push_back(CDomain_parent::eParent_type_classical);
        } else if (pChild->IsSetAncestors()){

            const CCdd::TAncestors & parents = pChild->GetAncestors();
            CCdd::TAncestors::const_iterator parentIt;
	        for(parentIt = parents.begin(); parentIt != parents.end(); parentIt++){
                if (parentIt->NotEmpty()) {
                    parentTypes.push_back((CDomain_parent::EParent_type) (*parentIt)->GetParent_type());
                }
            }
        }
    }

    return parentTypes.size();
}

int algAssignComponentParent(CCdCore * pCD, CCdCore * pNewParentCD,CCdCore * pOldParentCD,int mergeMethod,int parType,string & lMsg)
{
	int cnt=0;
		
    // put the old parent alignment as one of complex parents
    if( !pCD->IsSetAncestors() && pOldParentCD ){
		// remove the old classical parent and install it as a new one
		pCD->ResetParent();
		cnt+=addCDComponentParent(pCD, pOldParentCD ,mergeMethod,parType,lMsg);
    }
	
	cnt+=addCDComponentParent(pCD, pNewParentCD ,mergeMethod,parType,lMsg);
	fixParentType(pCD);
	return cnt;
}

bool algRemoveComponentParent(CCdCore * pCD, CCdCore * pOldParentCD,string & lMsg)
{
	CRef<CCdd_id> OldParentId=(*(pOldParentCD->GetId().Get().begin()) );
	bool res=removeComplexParent(pCD, OldParentId);
		// cleanup
	fixParentType(pCD);
	return res;
}


CCdCore* getComponentParentByIndex(const CCdCore * pSrcCD, int guideAlignmentIndex)
{
    vector<string> parentNames;
    int nParents = getComponentParents(pSrcCD, NULL, &parentNames, true);

    if (guideAlignmentIndex >=0 && guideAlignmentIndex < nParents) {
        return m_Data.getCD(parentNames[guideAlignmentIndex].c_str());
    }

    return NULL;
}

int getComponentParents(const CCdCore * pSrcCD, string * allNames, vector < CCdCore* > * pars, bool returnAll)
{
    vector<string> parentNames;
    int result = getComponentParents(pSrcCD, allNames, &parentNames, returnAll);

    if (pars) {
        for (int i = 0; i < result; ++i) {
            pars->push_back(m_Data.getCD(parentNames[i].c_str()));
        }
    }

    return result;
}

int getComponentParents(const CCdCore * pSrcCD, string * allNames, vector < string > * pars, bool returnAll)
{
    if(!pSrcCD || !pSrcCD->IsSetAncestors()){
        return 0;
    }

    int cnt,j;
	const list< CRef< CDomain_parent > > & fus=pSrcCD->GetAncestors();
    list< CRef< CDomain_parent > >::const_iterator iDom;
	
	for(cnt=0, iDom=fus.begin(); iDom!=fus.end(); iDom++,cnt++){
		const CRef< CDomain_parent > &dom=(*iDom);
		string par=dom->GetParentid().GetGid().GetAccession();
        if(pars){
			for(j=0; j<pars->size(); ++j)
				if(!returnAll && par==(*pars)[j])break;
			if(j==pars->size()) // first time (unless returnAll == true)
				pars->push_back(par);
		}
        if(allNames){
			if( !returnAll && !strstr( allNames->c_str() , string("__"+par+"__").c_str())  )// first time (unless returnAll == true)
				(*allNames)+="__"+par+"__";
		}
    }
    
    return cnt;
}

int getComponentChildren(CCdCore * pSrcCD, string * allNames, vector < CCdCore* > * children)
{
    vector<string> childNames;
    int result = getComponentChildren(pSrcCD, allNames, &childNames);

    if (children) {
        for (int i = 0; i < result; ++i) {
            children->push_back(m_Data.getCD(childNames[i].c_str()));
        }
    }

    return result;
}

int getComponentChildren(CCdCore * pSrcCD, string * allNames, vector < string > * children)
{
    if(!pSrcCD){
        return 0;
    }

    vector <const CCdd_id* > ids;
    vector < CCdCore * > cds;
	m_Data.getAllCDs(&cds);

    int nIds = 0;
    int nCDs = cds.size();
    string srcAccession = pSrcCD->GetAccession();

    int i, j, cnt = 0;
	
    if (children) children->clear();

    for (j = 0; j < nCDs; ++j) {
        if (cds[j] && cds[j]->GetComponentParentIds(ids)) {
            nIds = ids.size();
            for (i = 0; i < nIds; ++i) {
                if (srcAccession == ids[i]->GetGid().GetAccession()) {
                    ++cnt;
                    if (children) {  // && find(pars->begin(), pars->end(), cds[j]->GetAccession()) == pars->end()) {
                        children->push_back(cds[j]->GetAccession());
                    }
                    if(allNames){
				        (*allNames)+="__" + cds[j]->GetAccession() + "__";
		            }

                }
            }
        }
    }
    return cnt;
}

bool isClassicalParent(const CCdCore* potentialClassicalParent, const CCdCore* childCD) {
    return (getParentType(potentialClassicalParent, childCD) == CDomain_parent::eParent_type_classical);
}


bool isComponentParentType(CDomain_parent::EParent_type type) {
    return ((type != CDomain_parent::eParent_type_other) &&
            (type != CDomain_parent::eParent_type_classical));
}


bool isComponentParent(const CCdCore* potentialCompParent, const CCdCore* childCD) {

    bool result = false;
    vector<string> parents;
    if (getComponentParents(childCD, NULL, &parents) > 0) {
        for (int i = 0; i < parents.size(); ++i) {
            if (parents[i] == potentialCompParent->GetAccession()) {
                if (isComponentParentType(getParentType(potentialCompParent, childCD))) {
                    result = true;
                }
            }
        }
    }
    return result;
}

bool isComponentChild(const CCdCore* potentialCompChild, const CCdCore* parentCD) {

    return isComponentParent(parentCD, potentialCompChild);
}

bool hasParentChildRelationship(const CCdCore* potentialParent, const CCdCore* potentialChild) {
    return (isClassicalParent(potentialParent, potentialChild) || 
            isComponentParent(potentialParent, potentialChild));
}


CDomain_parent * findComponentParent(CCdCore * pSrcCD, string acc, int * pCnt)
{
    if(pCnt)*pCnt=0;
    if(!pSrcCD->IsSetAncestors()){
        return 0;
    }

	int cnt;
	CDomain_parent * retDom=0;
	list< CRef< CDomain_parent > > & fus=pSrcCD->SetAncestors();
    list< CRef< CDomain_parent > >::iterator iDom;
	for(cnt=0, iDom=fus.begin(); iDom!=fus.end(); iDom++){
		CRef< CDomain_parent > &dom=(*iDom);
		string par=dom->GetParentid().GetGid().GetAccession();
        if(par==acc){
			if(!retDom)retDom=&(*dom);
			//return &(*dom);
			cnt++;
		}
    }
		
	if(pCnt)*pCnt=cnt;
	return retDom;
}


//   Retrieve the seq_align for the index-th component parent. 
//   False if a) no such index, b) there is no guide alignment for index,
//   c) CD has a classical parent.
bool getGuideAlignment(const CCdCore* pCD, int index, CRef< CSeq_align >& seqAlign) {

    bool result = false;
    list< CRef< CDomain_parent > >::const_iterator iDom;

    seqAlign.Reset();
    if (pCD && pCD->IsSetAncestors()) {
        int i = 0;
        const list< CRef< CDomain_parent > >& parents = pCD->GetAncestors();
        for (iDom = parents.begin(); iDom != parents.end(); ++i, ++iDom) {
            if (i == index) {
                if ((*iDom)->IsSetSeqannot() && (*iDom)->GetSeqannot().GetData().IsAlign()) {
                    seqAlign = (*iDom)->GetSeqannot().GetData().GetAlign().front();
//                    seqAlign->Assign(*((*iDom)->GetSeqannot().GetData().GetAlign().front()));
                    result = true;
                }
                break;
            }
        }
    }

    return result;
}

bool getGuideAlignmentByRepeat(const CCdCore* pParent, const CCdCore* pChild, CRef< CSeq_align >& seqAlign, int nThRepeat) {
    bool result = false;
    vector < CCdCore* > parents;
    int nRepeats = 0;
    int nParents = getComponentParents(pChild, NULL, &parents, true);

    seqAlign.Reset();
    if (nThRepeat >= 1) {
        for (int i = 0; i < nParents; ++i) {
            if (parents[i] == pParent) {
                ++nRepeats;
                if (nRepeats == nThRepeat) {
                    result = getGuideAlignment(pChild, i, seqAlign);
					break;
                }
            }
        }
    }
    return result;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


