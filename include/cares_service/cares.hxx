#ifndef __CARES_SERVICES_CARES_HXX__
#define __CARES_SERVICES_CARES_HXX__

#include "detail/io_object.hxx"
#include "detail/service.hxx"
#include "detail/error.hxx"

namespace cares {

template<class Protocol>
using resolver = detail::basic_cares_resolver<detail::base_cares_service<Protocol>>;

namespace tcp {
using resolver = ::cares::resolver<boost::asio::ip::tcp>;
} // namespace tcp

namespace udp {
using resolver = ::cares::resolver<boost::asio::ip::udp>;
} // namespace udp

using detail::available_resolve_modes;

} // namespace cares

#endif // __CARES_SERVICES_CARES_HXX__
