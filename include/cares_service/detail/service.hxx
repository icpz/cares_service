#ifndef __CARES_SERVICES_SERVICES_HXX__
#define __CARES_SERVICES_SERVICES_HXX__

#include <memory>
#include <boost/asio.hpp>
#include "error.hxx"
#include "channel.hxx"
#include "endpoint_sequence.hxx"

namespace cares {
namespace detail {

template<class Protocol, class ChannelImplementation = Channel>
class base_cares_service : public boost::asio::io_context::service {
public:
    using implementation_type = std::shared_ptr<ChannelImplementation>;
    using results_type = EndpointSequence<Protocol>;
    using resolve_handler = std::function<void(boost::system::error_code, results_type)>;

    static boost::asio::io_context::id id;

    explicit base_cares_service(boost::asio::io_context &context)
        : boost::asio::io_context::service(context) {
        int ret = ::ares_library_init(ARES_LIB_INIT_ALL);
        if (ret != ARES_SUCCESS) {
            boost::system::error_code ec{ret, error::get_category()};
            boost::throw_exception(
                boost::system::system_error{
                    ec, ::ares_strerror(ret)
                }
            );
        }
    }

    ~base_cares_service() {
    }

    void shutdown() {
        ::ares_library_cleanup();
    }

    void construct(implementation_type &impl) {
        impl = std::make_shared<ChannelImplementation>(get_io_context());
    }

    void destroy(implementation_type &impl) {
        impl->Cancel();
        impl.reset();
    }

    template<class Handler>
    void async_resolve(implementation_type &impl, const std::string &name, uint16_t port, Handler &&cb) {
        auto handler = std::make_shared<Handler>(std::move(cb));
        auto result = std::make_shared<results_type>(port);
        auto wrapper = \
            [impl, handler, result, this]
            (boost::system::error_code ec, struct hostent *entries) {
                if (!ec) {
                    result->Append(entries);
                }
                if (result.use_count() == 1) {
                    if (!result->IsEmpty()) {
                        ec.clear();
                    }
                    boost::asio::post(
                        get_io_context(),
                        [handler, ec, result](){
                            (*handler)(ec, *result);
                        }
                    );
                }
            };
        impl->AsyncGetHostByName(name, AF_INET, wrapper);
        impl->AsyncGetHostByName(name, AF_INET6, wrapper);
    }

    void cancel(implementation_type &impl) {
        impl->Cancel();
    }

    void set_servers(implementation_type &impl, const std::string &servers, boost::system::error_code &ec) {
        impl->SetServerPortsCsv(servers, ec);
    }
};

template<class Protocol, class ChannelImplementation>
boost::asio::io_context::id base_cares_service<Protocol, ChannelImplementation>::id;

} // namespace detail
} // namespace cares

#endif // __CARES_SERVICES_SERVICES_HXX__
