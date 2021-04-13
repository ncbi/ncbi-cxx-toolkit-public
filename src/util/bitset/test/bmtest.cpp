#include <ncbi_pch.hpp>
#include <ncbiconf.h>
#ifdef BM64ADDR
#  include "bmtest64.cpp"
#else
#  include "bmtest32.cpp"
#endif
