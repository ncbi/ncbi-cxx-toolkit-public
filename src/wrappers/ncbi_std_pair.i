// Very simple wrapping for std::pair

namespace std {

template<class T, class U> struct pair
{
    T first;
    U second;
};

}
