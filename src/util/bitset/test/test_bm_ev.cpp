#include <ncbi_pch.hpp>

#include <util/bitset/bm.h>
#include <util/bitset/bmserial.h>
#include <util/simple_buffer.hpp>

#include <common/test_assert.h>

USING_NCBI_SCOPE;

typedef bm::bvector<> bvector;

static const size_t ids[] = {
    1, 1171
};

void Fill(bvector& bv)
{
    for ( auto id : ids ) {
        bv.set(id, true);
    }
}

static CSimpleBuffer buffer;
static bm::word_t* tmp_block = 0;

void Serialize(const bvector& bv, const bvector::statistics* in_stat = nullptr)
{
    bvector::statistics tmp_stat;
    if (!in_stat) {
        bv.calc_stat(&tmp_stat);
        in_stat = &tmp_stat;
    }

    LOG_POST("max_serialize_mem: "<<in_stat->max_serialize_mem);
    if (in_stat->max_serialize_mem > buffer.size()) {
        buffer.resize(in_stat->max_serialize_mem);
    }
    if ( !tmp_block ) {
        tmp_block = (bm::word_t*)bm::aligned_new_malloc(bm::set_block_alloc_size);
    }
    size_t size = bm::serialize(bv, (unsigned char*)&buffer[0],
                                tmp_block, bm::BM_NO_BYTE_ORDER);
    buffer.resize(size);
    LOG_POST("Serialized size: "<<size);
}

int main(void)
{
    bvector bv1(bm::BM_GAP);
    Fill(bv1);
    Serialize(bv1);
    return 0;
}
