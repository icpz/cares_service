#ifndef __CARES_SERVICES_IO_OBJECT_HXX__
#define __CARES_SERVICES_IO_OBJECT_HXX__

#include <boost/asio.hpp>

namespace cares {
namespace detail {

template<class Service>
class basic_cares_resolver : public boost::asio::basic_io_object<Service> {
public:
    using results_type = typename Service::results_type;
    using native_handle_type = typename Service::native_handle_type;
    using resolve_mode_type = typename Service::resolve_mode_type;

    explicit basic_cares_resolver(boost::asio::io_context &context)
        : boost::asio::basic_io_object<Service>(context) {
    }

    template<class Handler>
    void async_resolve(const std::string &name, uint16_t port, Handler &&cb) {
        this->get_service().async_resolve(this->get_implementation(), name, port, std::forward<Handler>(cb));
    }

    void cancel() {
        this->get_service().cancel(this->get_implementation());
    }

    void set_servers(const std::string &servers, boost::system::error_code &ec) {
        this->get_service().set_servers(this->get_implementation(), servers, ec);
    }

    resolve_mode_type resolve_mode() {
        return this->get_service().resolve_mode(this->get_implementation());
    }

    void resolve_mode(resolve_mode_type mode, boost::system::error_code &ec) {
        this->get_service().resolve_mode(this->get_implementation(), mode, ec);
    }

    native_handle_type native_handle() {
        return this->get_service().native_handle(this->get_implementation());
    }

};

} // namespace detail
} // namespace cares

#endif // __CARES_SERVICES_IO_OBJECT_HXX__
