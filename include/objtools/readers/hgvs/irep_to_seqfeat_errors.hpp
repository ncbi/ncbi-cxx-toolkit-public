#ifndef _IREP_TO_SEQFEAT_ERRORS_HPP_
#define _IREP_TO_SEQFEAT_ERRORS_HPP_

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_message.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CVariationDeltaException : public CException
{
public: 
    enum EErrCode {
       eUnknownLength,
       eInvalidLength,
    };

    virtual const char* GetErrCodeString(void) const; 
    NCBI_EXCEPTION_DEFAULT(CVariationDeltaException,CException);
};


class CVariationIrepException : public CException
{
public:
    enum EErrCode {
        eInvalidLocation,
        eInvalidInterval,
        eInvalidInsertion,
        eInvalidCount,
        eUnknownVariation, // Unrecognized variation type
        eInvalidVariation,  // CVarDesc data not consistent with variation type 
        eNullScope,
        eInvalidSeqType,
        eInvalidSeqId,
        eCDSError,
        eUnsupported
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CVariationIrepException,CException);
};


// Maybe I should separate the messages into two classes
// I could havs HgvsIrepMessage and HgvsSeqfeatMessage. 
// Maybe...
class CSeq_loc;
class CSeq_literal;
class CDelta_item;

class CVariationIrepMessage : public CMessage_Basic
{
public:
    
    CVariationIrepMessage(const string& msg,
                     EDiagSev      sev,
                     int           err_code=0);

    virtual ~CVariationIrepMessage(void);

    virtual CVariationIrepMessage* Clone(void) const;

    virtual void Write(CNcbiOstream& out) const;

    enum EObjectType {
        eNot_set,
        eAaloc,
        eAaint,
        eNtloc,
        eNtint,
        eNtsite,
        eRange,
        eSeq_loc,
        eSeq_literal,
        eDelta_item
    }; // Not sure if this is the right choice of types

    EObjectType Which(void) const { return m_ObjType; }
    
    void SetAaLocation(const CAaLocation& aa_loc);
    const CAaLocation* GetAaLocation(void) const;

    void SetAaInterval(const CAaInterval& aa_int);
    const CAaInterval* GetAaInterval(void) const;

    void SetNtLocation(const CNtLocation& nt_loc);
    const CNtLocation* GetNtLocation(void) const;

    void SetNtInterval(const CNtInterval& nt_int);
    const CNtInterval* GetNtInterval(void) const;

    void SetNtSite(const CNtSite& nt_site);
    const CNtSite* GetNtSite(void) const;

    void SetRange(const CCount::TRange& range);
    const CCount::TRange* GetRange(void) const;

    void SetLoc(const CSeq_loc& loc);
    const CSeq_loc* GetLoc(void) const;

    void SetLiteral(const CSeq_literal& literal);
    const CSeq_literal* GetLiteral(void) const;

    void SetDelta(const CDelta_item& delta_item);
    const CDelta_item* GetDelta(void) const;

    // Set the stored object to null
    void ResetObject(void);

private:

    template<typename T> 
    void xSetObj(const T& obj);
    
    template<typename T>
    const T* xGetObjPointer(void) const;

    EObjectType m_ObjType;
    CRef<CObject> m_Obj;
};



class CVariationIrepMessageListener : public CMessageListener_Basic
{
};


class CVariationValidateException : public CException
{
public: 
    enum EErrCode {
      eIDResolveError,
      eSeqliteralIntervalError
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CVariationValidateException,CException);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _IREP_TO_SEQFEAT_ERRORS_HPP_
