#ifndef STRUCT_CMT_READER_HPP
#define STRUCT_CMT_READER_HPP

#include <corelib/ncbistl.hpp>

#include <objtools/readers/struct_cmt_reader.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CUser_object;
    class CSeq_descr;
    class CBioseq;
    class CObject_id;
    class CSeq_entry;
    class ILineErrorListener;
}

class CSerialObject;
class ILineReader;

class CTable2AsnStructuredCommentsReader : public CStructuredCommentsReader
{
public:
    // If you need messages and error to be logged
    // supply an optional ILineErrorListener instance
    CTable2AsnStructuredCommentsReader(const std::string& filename, objects::ILineErrorListener* logger);
    ~CTable2AsnStructuredCommentsReader();
    void ProcessComments(objects::CSeq_entry& entry) const;
private:
    // Read input tab delimited file and apply Structured Comments to the container
    // First row of the file is a list of Field to apply
    // First collumn of the file is an ID of the object (sequence) to apply
    // Or
    // Read input tab delimited file and apply Structured Comments to all objects
    // (sequences) in the container
    // First collumn of the file is the name of the field
    // Second collumn of the file is the value of the field

    static
    bool IsVertical(ILineReader& reader);

    static void _AddStructuredComments(objects::CSeq_entry& entry, const CStructComment& comments);
    static void _AddStructuredComments(objects::CSeq_descr& descr, const CStructComment& comments);

    static void _CheckStructuredCommentsSuffix(CStructComment& comments);
    list<CStructComment> m_comments;
    bool m_vertical = false;
};

END_NCBI_SCOPE

#endif
