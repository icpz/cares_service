#ifndef __CARES_SERVICE_DETAIL_RESOLVE_MODE_HXX__
#define __CARES_SERVICE_DETAIL_RESOLVE_MODE_HXX__

#include <string>
#include <vector>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#if defined(__CARES_RESOLVE_STR2MODE) || defined(__CARES_RESOLVE_MODE_SEQ) || defined(__CARES_RESOLVE_MODE_ALLMODES)
#error Internal macro already defined
#endif // defined(__CARES_RESOLVE_STR2MODE) || defined(__CARES_RESOLVE_MODE_SEQ) || defined(__CARES_RESOLVE_MODE_ALLMODES)

namespace cares {
namespace detail {

#define __CARES_RESOLVE_MODE_SEQ \
    (unspecific)(ipv4_first)(ipv4_only)(ipv6_first)(ipv6_only)(both)

enum resolve_mode {
    BOOST_PP_SEQ_ENUM(__CARES_RESOLVE_MODE_SEQ)
};

inline bool resolve_mode_from_string(const std::string &str, resolve_mode &mode) {
#define __CARES_RESOLVE_STR2MODE(unused, data, elem) \
    do { if (data == BOOST_PP_STRINGIZE(elem)) { mode = elem; return true;} } while (false);

    BOOST_PP_SEQ_FOR_EACH(__CARES_RESOLVE_STR2MODE, str, __CARES_RESOLVE_MODE_SEQ);
    return false;
}

inline std::vector<std::string> available_resolve_modes() {
#define __CARES_RESOLVE_MODE_ALLMODES(unused, data, elem) \
    do { data.push_back(BOOST_PP_STRINGIZE(elem)); } while (false);

    std::vector<std::string> result;
    BOOST_PP_SEQ_FOR_EACH(__CARES_RESOLVE_MODE_ALLMODES, result, __CARES_RESOLVE_MODE_SEQ);
    return result;
}

#undef __CARES_RESOLVE_MODE_ALLMODES
#undef __CARES_RESOLVE_STR2MODE
#undef __CARES_RESOLVE_MODE_SEQ

} // namespace detail
} // namespace cares

#endif // __CARES_SERVICE_DETAIL_RESOLVE_MODE_HXX__
