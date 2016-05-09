#include <ncbi_pch.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const char* CVariationDeltaException::GetErrCodeString(void) const
{
    switch( GetErrCode() ) {
    case eUnknownLength:    return "eUnknownLength";
    case eInvalidLength:    return "eInvalidLength";
    default:                return CException::GetErrCodeString();
    }
}


const char* CVariationIrepException::GetErrCodeString(void) const 
{
    switch( GetErrCode() ) {
        case eInvalidLocation:  return "eInvalidLocation";
        case eInvalidInterval:  return "eInvalidInterval";
        case eInvalidCount:     return "eInvalidCount";
        case eInvalidInsertion: return "eInvalidInsertion";
        case eUnknownVariation: return "eUnknownVariation";
        case eInvalidVariation: return "eInvalidVariation";
        case eNullScope:        return "eNullScope";
        case eInvalidSeqType:   return "eInvalidSeqType";
        case eInvalidSeqId:     return "eInvalidSeqId";
        case eUnsupported:      return "eUnsupported";
        case eCDSError:         return "eCDSError";
        default:                return  CException::GetErrCodeString();
    }
}


// Constructor
CVariationIrepMessage::CVariationIrepMessage(const string& msg,
                                   EDiagSev      sev,
                                   int           err_code)
    : CMessage_Basic(msg, sev, err_code),
      m_ObjType(eNot_set),
      m_Obj(null)
{
}


// Destructor 
CVariationIrepMessage::~CVariationIrepMessage(void)
{
}


// Clone
CVariationIrepMessage* CVariationIrepMessage::Clone(void) const
{
    return new CVariationIrepMessage(*this);
}


void CVariationIrepMessage::Write(CNcbiOstream& out) const
{
    CMessage_Basic::Write(out);
    switch ( Which() ) {
    case eNot_set:
        out << "NULL"; 
        break;
    case eAaloc:
        out << MSerial_AsnText << *GetAaLocation();
        break;
    case eAaint:
        out << MSerial_AsnText << *GetAaInterval();
        break;
    case eNtloc:
        out << MSerial_AsnText << *GetNtLocation();
        break;
    case eNtint:
        out << MSerial_AsnText << *GetNtInterval();
        break;
    case eNtsite:
        out << MSerial_AsnText << *GetNtSite();
        break;
    case eSeq_loc:
        out << MSerial_AsnText << *GetLoc();
        break;
    case eSeq_literal:
        out << MSerial_AsnText << *GetLiteral();
        break;
    case eDelta_item:
        out << MSerial_AsnText << *GetDelta();
        break;
    default:
        out << "Invalid message type";
        break;
    }
}


template<typename T>
void CVariationIrepMessage::xSetObj(const T& obj)
{
    auto ref = Ref(new T());
    ref->Assign(obj);
    m_Obj = ref;
}


template<typename T>
const T* CVariationIrepMessage::xGetObjPointer(void) const
{
    return dynamic_cast<const T*>(m_Obj.GetPointerOrNull());
}


void CVariationIrepMessage::SetAaLocation(const CAaLocation& aa_loc)
{
    m_ObjType = eAaloc;
    xSetObj(aa_loc);
}


const CAaLocation* CVariationIrepMessage::GetAaLocation(void) const 
{
    return xGetObjPointer<CAaLocation>();
}


void CVariationIrepMessage::SetAaInterval(const CAaInterval& aa_int)
{
    m_ObjType = eAaint;
    xSetObj(aa_int);
}


const CAaInterval* CVariationIrepMessage::GetAaInterval(void) const 
{
    return xGetObjPointer<CAaInterval>();
}


void CVariationIrepMessage::SetNtLocation(const CNtLocation& nt_loc)
{
    m_ObjType = eNtloc;
    xSetObj(nt_loc);
}


const CNtLocation* CVariationIrepMessage::GetNtLocation(void) const
{
    return xGetObjPointer<CNtLocation>();
}


void CVariationIrepMessage::SetNtInterval(const CNtInterval& nt_int)
{
    m_ObjType = eNtint;
    xSetObj(nt_int);
}


const CNtInterval* CVariationIrepMessage::GetNtInterval(void) const 
{
    return xGetObjPointer<CNtInterval>();
}


void CVariationIrepMessage::SetNtSite(const CNtSite& nt_site)
{
    m_ObjType = eNtsite;
    xSetObj(nt_site);
}


const CNtSite* CVariationIrepMessage::GetNtSite(void) const
{
    return xGetObjPointer<CNtSite>();
}


void CVariationIrepMessage::SetRange(const CCount::TRange& range)
{
    m_ObjType = eRange;
    xSetObj(range);
}


const CCount::TRange* CVariationIrepMessage::GetRange(void) const
{
    return xGetObjPointer<CCount::TRange>();
}


void CVariationIrepMessage::SetLoc(const CSeq_loc& loc)
{
    m_ObjType = eSeq_loc;
    xSetObj(loc);
}


const CSeq_loc* CVariationIrepMessage::GetLoc(void) const 
{
    return xGetObjPointer<CSeq_loc>();
}


void CVariationIrepMessage::SetLiteral(const CSeq_literal& lit)
{
    m_ObjType = eSeq_literal;
    xSetObj(lit);
}


const CSeq_literal* CVariationIrepMessage::GetLiteral(void) const 
{
    return xGetObjPointer<CSeq_literal>();
}


void CVariationIrepMessage::SetDelta(const CDelta_item& delta_item)
{
    m_ObjType = eDelta_item;
    xSetObj(delta_item);
}


const CDelta_item* CVariationIrepMessage::GetDelta(void) const 
{
    return xGetObjPointer<CDelta_item>();
}


const char* CVariationValidateException::GetErrCodeString(void) const 
{
    switch( GetErrCode() ) {
    case eIDResolveError: return "eIDResolveError";
    case eSeqliteralIntervalError: return "eSeqliteralIntervalError";
    default:              return CException::GetErrCodeString();
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
