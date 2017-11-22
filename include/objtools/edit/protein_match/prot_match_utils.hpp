#ifndef _PROT_MATCH_UTILS_HPP_
#define _PROT_MATCH_UTILS_HPP_

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SIdCompare
{
    bool operator()(const CRef<CSeq_id>& id1,
        const CRef<CSeq_id>& id2) const 
    {
        return id1->CompareOrdered(*id2) < 0;
    }
};
// Should have CSOverwriteIdInfo
struct SOverwriteIdInfo {

    string update_nuc_id;
    list<string> update_prot_ids;

    list<string> replaced_nuc_ids;
    list<string> replaced_prot_ids;

    map<string, list<string>> replaced_prot_id_map;

    list<string> db_nuc_ids;
    map<string, list<string>> db_prot_ids;

    bool IsReplacedNucId(const string& id) const {
        return (find(replaced_nuc_ids.begin(), replaced_nuc_ids.end(), id) != replaced_nuc_ids.end());
    }

    bool DBEntryHasProteins(const string& db_nuc_id) const {
        return  db_prot_ids.find(db_nuc_id) != db_prot_ids.end();
    }

    bool ReplacesEntry(void) const {
        return !replaced_nuc_ids.empty();
    }
};


class CNucProtInfo {
private:
    string m_NucId;
    list<string> m_ProtIds;

public:
    string& SetNucId(void) { return m_NucId; }
    const string& GetNucId(void) { return m_NucId; }
    bool IsSetNucId(void) const { return !m_NucId.empty(); }

    list<string>& SetProtIds(void) { return m_ProtIds; }
    const list<string>& GetProtIds(void) { return m_ProtIds; }
    bool IsSetProtIds(void) const { return !m_ProtIds.empty(); }
};



class CUpdateProtIds {

private:
    string m_Accession;
    string m_Gi;

    list<CRef<CSeq_id>> m_Others;
public:

    using TOtherIds = list<CRef<CSeq_id>>;


    bool IsSetAccession(void) const {
        return !NStr::IsBlank(m_Accession);
    }

    bool IsSetGi(void) const {
        return !NStr::IsBlank(m_Gi); 
    }

    bool IsSetOthers(void) const {
        return !(m_Others.empty());
    }


    string& SetAccession(void) {
        return m_Accession;
    }

    string& SetGi(void) {
        return m_Gi;
    }

    TOtherIds& SetOthers(void) {
        return m_Others;
    }

    const string& GetAccession(void) const {
        return m_Accession;
    }

    const string& GetGi(void) const {
        return m_Gi;
    }

    const TOtherIds& GetOthers(void) const {
        return m_Others;
    }
};



class CMatchIdInfo {
 private:
     string m_UpdateNucId;
     list<string> m_UpdateOtherProtIds;


     list<list<CRef<CSeq_id>>> m_UpdateProtIds;
     
     string m_DBNucId;
     list<string> m_DBProtIds;

public:

     string& SetUpdateNucId(void) { return m_UpdateNucId; }
     const string& GetUpdateNucId(void) const { return m_UpdateNucId; }


     list<list<CRef<CSeq_id>>>& SetUpdateProtIds(void) { return m_UpdateProtIds; }
     const list<list<CRef<CSeq_id>>>& GetUpdateProtIds(void) const { return m_UpdateProtIds; }
     const bool IsSetUpdateProtIds(void) const { return !m_UpdateProtIds.empty(); }

     list<string>& SetUpdateOtherProtIds(void) { return m_UpdateOtherProtIds; }
     const list<string>& GetUpdateOtherProtIds(void) const { return m_UpdateOtherProtIds; }

     string& SetDBNucId(void) { return m_DBNucId; }
     const string& GetDBNucId(void) const { return m_DBNucId; }

     list<string>& SetDBProtIds(void) { return m_DBProtIds; }
     const list<string>& GetDBProtIds(void) const { return m_DBProtIds; }


     bool NucIdChanges(void) const { return m_UpdateNucId != m_DBNucId; }
     bool DBEntryHasProts(void) const { return !m_DBProtIds.empty(); }

     bool IsDBProtId(const string& prot_id) const { 
        return (find(m_DBProtIds.begin(), m_DBProtIds.end(), prot_id) != m_DBProtIds.end());
     }

     bool HasLocalProtIds(void) const {
        return !m_UpdateOtherProtIds.empty();
     }

     bool IsOtherProtId(const string& prot_id) const { 
        return (find(m_UpdateOtherProtIds.begin(), m_UpdateOtherProtIds.end(), prot_id) != m_UpdateOtherProtIds.end());
     }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _PROT_MATCH_UTILS_
