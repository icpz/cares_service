#ifndef __CARES_SERVICES_EPSEQ_HXX__
#define __CARES_SERVICES_EPSEQ_HXX__

#include <list>
#include <memory>
#include <ares.h>
#include <boost/asio.hpp>

namespace cares {
namespace detail {

template<class Protocol>
class EndpointSequence {
    using address = boost::asio::ip::address;
public:
    using endpoint = boost::asio::ip::basic_endpoint<Protocol>;
    using sequence = std::list<endpoint>;
    using iterator = typename sequence::iterator;
    using const_iterator = typename sequence::const_iterator;

    explicit EndpointSequence(uint16_t port)
        : endpoints_(std::make_shared<sequence>()), last_family_(AF_UNSPEC), port_(port) {
    }

    EndpointSequence(const struct hostent *entries, uint16_t port)
        : EndpointSequence(port) {
        Append(entries);
    }

    EndpointSequence(const EndpointSequence &) = default;

    ~EndpointSequence() = default;

    void Append(const struct hostent *entries) {
        sequence subseq;
        BuildList(entries, subseq);
        if (!subseq.empty()) {
            last_family_ = entries->h_addrtype;
            endpoints_->splice(endpoints_->end(), subseq);
        }
    }

    void Append(boost::asio::ip::address address) {
        last_family_ = (address.is_v4() ? AF_INET : AF_INET6);
        endpoints_->emplace_back(std::move(address), port_);
    }

    void Prepend(const struct hostent *entries) {
        sequence subseq;
        BuildList(entries, subseq);
        if (!subseq.empty()) {
            last_family_ = entries->h_addrtype;
            endpoints_->splice(endpoints_->begin(), subseq);
        }
    }

    bool LastFamily(int &family) const {
        if (last_family_ == AF_UNSPEC) {
            return false;
        }
        family = last_family_;
        return true;
    }

    bool IsEmpty() const {
        return endpoints_->empty();
    }

    iterator begin() {
        return endpoints_->begin();
    }

    const_iterator begin() const {
        return endpoints_->begin();
    }

    iterator end() {
        return endpoints_->end();
    }

    const_iterator end() const {
        return endpoints_->end();
    }

    bool empty() const {
        return IsEmpty();
    }

private:
    void BuildList(const struct hostent *entries, sequence &subseq) {
        address addr;
        subseq.clear();
        for (char **p = entries->h_addr_list; *p; ++p) {
            if (entries->h_addrtype == AF_INET) {
                boost::asio::ip::address_v4::bytes_type bytes;
                assert(bytes.size() == entries->h_length);
                memcpy(bytes.data(), *p, bytes.size());
                addr = boost::asio::ip::make_address_v4(bytes);
            } else {
                boost::asio::ip::address_v6::bytes_type bytes;
                assert(bytes.size() == entries->h_length);
                memcpy(bytes.data(), *p, bytes.size());
                addr = boost::asio::ip::make_address_v6(bytes);
            }
            subseq.emplace_back(std::move(addr), port_);
        }
    }

    std::shared_ptr<sequence> endpoints_;
    int last_family_;
    uint16_t port_;
};

}; // namespace detail
}; // namespace cares

#endif // __CARES_SERVICES_EPSEQ_HXX__

