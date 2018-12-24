#ifndef __CARES_SERVICES_ERROR_HXX__
#define __CARES_SERVICES_ERROR_HXX__

#include <memory>
#include <boost/system/error_code.hpp>

#include <ares.h>

namespace cares {
namespace error {

namespace detail {

class CaresErrorCategory : public boost::system::error_category {
public:
    const char *name() const noexcept {
        return "cares error";
    }

    std::string message(int ev) const {
        return ::ares_strerror(ev);
    }
};

} // namespace detail

inline boost::system::error_category &get_category() {
    static auto category = \
        std::make_unique<detail::CaresErrorCategory>();
    return *category;
}

enum basic_errors {
    no_data = ARES_ENODATA,
    malformat = ARES_EFORMERR,
    serve_failed = ARES_ESERVFAIL,
    not_found = ARES_ENOTFOUND,
    not_implemented = ARES_ENOTIMP,
    query_refused = ARES_EREFUSED,
    bad_query = ARES_EBADQUERY,
    bad_name = ARES_EBADNAME,
    bad_family = ARES_EBADFAMILY,
    bad_response = ARES_EBADRESP,
    connection_refused = ARES_ECONNREFUSED,
    timeout = ARES_ETIMEOUT,
    eof = ARES_EOF,
    configuration_file_error = ARES_EFILE,
    no_memory = ARES_ENOMEM,
    channel_destroyed = ARES_EDESTRUCTION,
    bad_string = ARES_EBADSTR,
    bad_flags = ARES_EBADFLAGS,
    no_name = ARES_ENONAME,
    bad_hints = ARES_EBADHINTS,
    not_initialized = ARES_ENOTINITIALIZED,
    iphlpapi_failed = ARES_ELOADIPHLPAPI,
    get_network_params_failed = ARES_EADDRGETNETWORKPARAMS,
    operation_cancelled = ARES_ECANCELLED
};

} // namespace error
} // namespace cares

namespace boost {
namespace system {

template<> struct is_error_code_enum<cares::error::basic_errors> {
    static const bool value = true;
};

} // namespace system
} // namespace boost

#endif // __CARES_SERVICES_ERROR_HXX__
