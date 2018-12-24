#ifndef __CARES_SERVICES_CHANNEL_HXX__
#define __CARES_SERVICES_CHANNEL_HXX__

#if defined(_WIN32) && !defined(__CYGWIN__)
    #define SET_SOCKERRNO(x) (WSASetLastError((int)(x)))
#else
    #define SET_SOCKERRNO(x) (errno = (x))
#endif

#include <memory>
#include <map>
#include <boost/asio.hpp>
#include <boost/variant.hpp>
#include <ares.h>

#include "error.hxx"

namespace cares {
namespace detail {

std::shared_ptr<struct ares_socket_functions> GetSocketFunctions();
char *GetAresLookups();

ares_socket_t OpenSocket(int family, int type, int protocol, void *arg);
int CloseSocket(ares_socket_t fd, void *arg);
int ConnectSocket(ares_socket_t fd, const struct sockaddr *addr, ares_socklen_t len, void *arg);
ares_ssize_t ReadSocket(ares_socket_t fd, void *data, size_t data_len, int flags, struct sockaddr *addr, ares_socklen_t *addr_len, void *arg);
ares_ssize_t SendSocket(ares_socket_t fd, const struct iovec *data, int len, void *arg);

void SocketStateCb(void *arg, ares_socket_t fd, int readable, int writeable);

class Channel : public std::enable_shared_from_this<Channel> {
    struct Socket : public std::enable_shared_from_this<Socket> {
        using tcp_type = boost::asio::ip::tcp::socket;
        using udp_type = boost::asio::ip::udp::socket;

        Socket(tcp_type tcp)
            : socket_(std::move(tcp)), is_tcp_(true) {
        }

        Socket(udp_type udp)
            : socket_(std::move(udp)), is_tcp_(false) {
        }

        Socket(Socket &&) = default;

        tcp_type &GetTcp() {
            return boost::get<tcp_type>(socket_);
        }

        udp_type &GetUdp() {
            return boost::get<udp_type>(socket_);
        }

        bool IsTcp() const {
            return is_tcp_;
        }

        ares_socket_t GetFd() {
            if (IsTcp()) {
                return GetTcp().native_handle();
            }
            return GetUdp().native_handle();
        }

        void Close() {
            if (IsTcp()) {
                GetTcp().close();
            } else {
                GetUdp().close();
            }
        }

        void Cancel() {
            if (IsTcp()) {
                GetTcp().cancel();
            } else {
                GetUdp().cancel();
            }
        }

        template<class Handler>
        void AsyncWaitRead(Handler &&cb) {
            auto self{shared_from_this()};
            auto handler = \
                [this, self, cb=std::move(cb)](boost::system::error_code ec) {
                    cb();
                    if (!ec) {
                        AsyncWaitRead(std::move(cb));
                    }
                };
            if (IsTcp()) {
                GetTcp().async_wait(tcp_type::wait_read, std::move(handler));
            } else {
                GetUdp().async_wait(udp_type::wait_read, std::move(handler));
            }
        }

        template<class Handler>
        void AsyncWaitWrite(Handler &&cb) {
            auto self{shared_from_this()};
            auto handler = \
                [this, self, cb=std::move(cb)](boost::system::error_code ec) {
                    cb();
                    if (!ec) {
                        AsyncWaitWrite(std::move(cb));
                    }
                };
            if (IsTcp()) {
                GetTcp().async_wait(tcp_type::wait_write, std::move(handler));
            } else {
                GetUdp().async_wait(udp_type::wait_write, std::move(handler));
            }
        }

        boost::variant<udp_type, tcp_type> socket_;
        bool is_tcp_;
    };
public:
    using AsyncCallback = std::function<void(boost::system::error_code, struct hostent *)>;

    Channel(const Channel &) = delete;
    Channel(boost::asio::io_context &ios)
        : context_(ios), timer_(context_), functions_(GetSocketFunctions()), request_count_(0) {
        struct ares_options option;
        memset(&option, 0, sizeof option);
        option.sock_state_cb = SocketStateCb;
        option.sock_state_cb_data = this;
        option.timeout = 3000;
        option.tries = 3;
        option.lookups = GetAresLookups();
        int mask = ARES_OPT_NOROTATE | ARES_OPT_TIMEOUTMS | ARES_OPT_SOCK_STATE_CB | ARES_OPT_TRIES | ARES_OPT_LOOKUPS;

        int ret = ::ares_init_options(&channel_, &option, mask);
        if (ret != ARES_SUCCESS) {
            boost::system::error_code ec{ret, error::get_category()};
            boost::throw_exception(
                boost::system::system_error{
                    ec, ::ares_strerror(ret)
                }
            );
        }

        ::ares_set_socket_functions(channel_, functions_.get(), this);
    }

    ~Channel() {
        Cancel();
        ::ares_destroy(channel_);
    }

private:
    struct ChannelComplete {
        std::shared_ptr<Channel> channel;
        AsyncCallback callback;
    };

public:
    template<class Callback>
    void AsyncGetHostByName(const std::string &domain, int family, Callback cb) {
        auto comp = std::make_unique<ChannelComplete>();
        comp->channel = shared_from_this();
        comp->callback = std::move(cb);
        ::ares_gethostbyname(channel_, domain.c_str(), family, &Channel::HostCallback, comp.release());
        if (request_count_ == 0) {
            TimerStart();
        }
        ++request_count_;
    }

    void Cancel() {
        ::ares_cancel(channel_);
        TimerStop();
    }

    void SetServerPortsCsv(const std::string &servers, boost::system::error_code &ec) {
        ec.clear();
        int ret = ::ares_set_servers_ports_csv(channel_, servers.c_str());
        if (ret != ARES_SUCCESS) {
            ec.assign(ret, error::get_category());
        }
    }

private:

    const uint64_t kTimerPeriod = 1000;

    void TimerStart() {
        TimerCallback(boost::system::error_code{});
    }

    void TimerStop() {
        timer_.cancel();
    }

    void TimerCallback(boost::system::error_code ec) {
        auto self{shared_from_this()};
        auto now = boost::posix_time::microsec_clock::local_time();
        auto after = now - last_tick_ + boost::posix_time::millisec{kTimerPeriod};
        if (after.is_negative() || after == boost::posix_time::millisec{0}) {
            last_tick_ = boost::posix_time::microsec_clock::local_time();
            ::ares_process_fd(channel_, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
            after = boost::posix_time::millisec{kTimerPeriod};
        }
        if (!ec) {
            timer_.expires_from_now(after);
            timer_.async_wait(
                std::bind(&Channel::TimerCallback, self, std::placeholders::_1)
            );
        }
    }

    static void HostCallback(void *arg, int status, int timeouts, struct hostent *hostent) {
        std::unique_ptr<ChannelComplete> comp;
        comp.reset(static_cast<ChannelComplete *>(arg));
        boost::system::error_code ec;
        if (status != ARES_SUCCESS) {
            ec.assign(status, error::get_category());
        }
        comp->callback(ec, hostent);
        comp->channel->request_count_--;
        if (comp->channel->request_count_ == 0) {
            comp->channel->TimerStop();
        }
    }

    boost::asio::io_context &context_;
    ares_channel channel_;
    boost::asio::deadline_timer timer_;
    boost::posix_time::ptime last_tick_;
    std::shared_ptr<struct ares_socket_functions> functions_;
    std::map<ares_socket_t, std::shared_ptr<Socket>> sockets_;
    uint64_t request_count_;

    friend ares_socket_t OpenSocket(int family, int type, int protocol, void *arg);
    friend int CloseSocket(ares_socket_t fd, void *arg);
    friend int ConnectSocket(ares_socket_t fd, const struct sockaddr *addr, ares_socklen_t len, void *arg);
    friend ares_ssize_t ReadSocket(ares_socket_t fd, void *data, size_t data_len, int flags, struct sockaddr *addr, ares_socklen_t *addr_len, void *arg);
    friend ares_ssize_t SendSocket(ares_socket_t fd, const struct iovec *data, int len, void *arg);
    friend void SocketStateCb(void *arg, ares_socket_t fd, int readable, int writeable);
};

std::shared_ptr<struct ares_socket_functions> GetSocketFunctions() {
    static std::shared_ptr<struct ares_socket_functions> funcs = \
        []() {
            auto result = std::make_shared<struct ares_socket_functions>();
            result->asocket   = OpenSocket;
            result->aclose    = CloseSocket;
            result->aconnect  = ConnectSocket;
            result->arecvfrom = ReadSocket;
            result->asendv    = SendSocket;
            return result;
        }();
    return funcs;
}

char *GetAresLookups() {
    static char lookups[] = "bf";
    return lookups;
}

ares_socket_t OpenSocket(int family, int type, int protocol, void *arg) {
    auto channel = static_cast<Channel *>(arg);
    auto &ios = channel->context_;
    ares_socket_t result = ARES_SOCKET_BAD;
    boost::system::error_code ec;

    if (type == SOCK_STREAM) {
        boost::asio::ip::tcp::socket sock{ios};
        auto af = (family == AF_INET) ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6();
        sock.open(af, ec);
        if (!ec) {
            sock.non_blocking(true);
            result = sock.native_handle();
            auto ptr = std::make_shared<Channel::Socket>(std::move(sock));
            channel->sockets_.emplace(result, ptr);
        }
    } else if (type == SOCK_DGRAM) {
        boost::asio::ip::udp::socket sock{ios};
        auto af = (family == AF_INET) ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6();
        sock.open(af, ec);
        if (!ec) {
            sock.non_blocking(true);
            result = sock.native_handle();
            auto ptr = std::make_shared<Channel::Socket>(std::move(sock));
            channel->sockets_.emplace(result, ptr);
        }
    } else {
        assert(false);
    }
    if (ec) {
        SET_SOCKERRNO(ec.value());
        return -1;
    }
    return result;
}

int CloseSocket(ares_socket_t fd, void *arg) {
    auto channel = static_cast<Channel *>(arg);
    auto &sockets = channel->sockets_;
    auto itr = sockets.find(fd);
    itr->second->Close();
    sockets.erase(itr);
    return 0;
}

int ConnectSocket(ares_socket_t fd, const struct sockaddr *addr, ares_socklen_t addr_len, void *arg) {
    auto channel = static_cast<Channel *>(arg);
    auto itr = channel->sockets_.find(fd);
    auto self{itr->second};
    boost::system::error_code ec;

    if (self->IsTcp()) {
        boost::asio::ip::tcp::endpoint ep;
        ep.resize(addr_len);
        memcpy(ep.data(), addr, addr_len);
        self->GetTcp().connect(ep, ec);
    } else {
        boost::asio::ip::udp::endpoint ep;
        ep.resize(addr_len);
        memcpy(ep.data(), addr, addr_len);
        self->GetUdp().connect(ep, ec);
    }
    SET_SOCKERRNO(ec.value());
    return (ec ? -1 : 0);
}

ares_ssize_t ReadSocket(ares_socket_t fd, void *data, size_t data_len, int flags, struct sockaddr *addr, ares_socklen_t *addr_len, void *arg) {
    auto channel = static_cast<Channel *>(arg);
    auto itr = channel->sockets_.find(fd);
    auto self{itr->second};
    boost::system::error_code ec;

    ares_ssize_t result = -1;
    if (self->IsTcp()) {
        auto &socket = self->GetTcp();
        result = socket.read_some(boost::asio::buffer(data, data_len), ec);
        if (!ec && addr) {
            auto ep = socket.remote_endpoint();
            *addr_len = ep.size();
            memcpy(addr, ep.data(), ep.size());
        }
    } else {
        auto &socket = self->GetUdp();
        boost::asio::ip::udp::endpoint ep;
        result = socket.receive_from(boost::asio::buffer(data, data_len), ep, flags, ec);
        if (!ec && addr) {
            *addr_len = ep.size();
            memcpy(addr, ep.data(), ep.size());
        }
    }
    SET_SOCKERRNO(ec.value());
    return (ec ? -1 : result);
}

ares_ssize_t SendSocket(ares_socket_t fd, const struct iovec *data, int len, void *arg) {
    auto channel = static_cast<Channel *>(arg);
    auto itr = channel->sockets_.find(fd);
    auto self{itr->second};
    boost::system::error_code ec;

    ares_ssize_t result = -1;
    std::vector<boost::asio::mutable_buffer> buf_seq;
    buf_seq.reserve(len);
    for (int i = 0; i < len; ++i) {
        buf_seq.push_back(boost::asio::buffer(data[i].iov_base, data[i].iov_len));
    }
    if (self->IsTcp()) {
        auto &socket = self->GetTcp();
        result = socket.write_some(std::move(buf_seq), ec);
    } else {
        auto &socket = self->GetUdp();
        result = socket.send(std::move(buf_seq), 0, ec);
    }
    SET_SOCKERRNO(ec.value());
    return (ec ? -1 : result);
}

void SocketStateCb(void *arg, ares_socket_t fd, int readable, int writeable) {
    auto channel = static_cast<Channel *>(arg);
    auto itr = channel->sockets_.find(fd);
    auto self{itr->second};

    self->Cancel();
    if (readable) {
        self->AsyncWaitRead(
            [channel, fd](){
                channel->last_tick_ = boost::posix_time::microsec_clock::local_time();
                ::ares_process_fd(channel->channel_, fd, ARES_SOCKET_BAD);
            }
        );
    }
    if (writeable) {
        self->AsyncWaitWrite(
            [channel, fd](){
                channel->last_tick_ = boost::posix_time::microsec_clock::local_time();
                ::ares_process_fd(channel->channel_, ARES_SOCKET_BAD, fd);
            }
        );
    }
}

} // namespace detail
} // namespace cares

#endif // __CARES_SERVICES_CHANNEL_HXX__

