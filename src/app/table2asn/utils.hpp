#ifndef __TABLE2ASN_UTILS_HPP
#define __TABLE2ASN_UTILS_HPP

#include <mutex>
#include <map>

BEGIN_NCBI_SCOPE

bool AssignLocalIdIfEmpty(ncbi::objects::CSeq_feat& feature, int& id);

class COnceMutex
{
public:
    void lock() {
        m_mutex.lock();
        auto visited = m_counter++;
        if (visited == 0)
            m_counter++;
        else
            m_mutex.unlock();
    }

    void unlock() {
        auto visited = m_counter--;
        if (visited == 2) {
            m_mutex.unlock();
        }
    }

    bool IsFirstRun() const {
        return 2 == m_counter.load();
    }

private:
    std::mutex          m_mutex;
    std::atomic<size_t> m_counter{0};
};

using TSharedStreamMap = std::map<std::string, std::tuple<std::string, unique_ptr<std::ostream>, std::mutex>>;

class CSharedOStream
{
public:
    CSharedOStream(TSharedStreamMap::mapped_type* owner);
    CSharedOStream(const CSharedOStream&) = delete;
    CSharedOStream(CSharedOStream&&) = default;
    CSharedOStream() = default;
    ~CSharedOStream();
    operator std::ostream&() { return get(); };
    std::ostream& get();
private:
    TSharedStreamMap::mapped_type* m_owner = nullptr;
};

END_NCBI_SCOPE

#endif
