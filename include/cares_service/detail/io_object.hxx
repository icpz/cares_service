#ifndef __CARES_SERVICES_IO_OBJECT_HXX__
#define __CARES_SERVICES_IO_OBJECT_HXX__

#include <boost/asio.hpp>

namespace cares {
namespace detail {

template<class Service>
class basic_cares_resolver : public boost::asio::basic_io_object<Service> {
public:
    using result_type = typename Service::result_type;

    explicit basic_cares_resolver(boost::asio::io_context &context)
        : boost::asio::basic_io_object<Service>(context) {
    }

    template<class Handler>
    void async_resolve(const std::string &name, uint16_t port, Handler cb) {
        this->get_service().async_resolve(this->get_implementation(), name, port, std::move(cb));
    }

    void cancel() {
        this->get_service().cancel(this->get_implementation());
    }

    void set_servers(const std::string &servers, boost::system::error_code &ec) {
        this->get_service().set_servers(this->get_implementation(), servers, ec);
    }
};

} // namespace detail
} // namespace cares

#endif // __CARES_SERVICES_IO_OBJECT_HXX__
