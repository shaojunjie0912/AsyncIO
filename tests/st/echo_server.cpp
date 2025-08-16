#include <arpa/inet.h>

#include <asyncio/asyncio.hpp>

using namespace asyncio;

int add_count = 0;
int rel_count = 0;

Task<> handle_echo(Stream stream) {
    auto sockinfo = stream.GetSockInfo();
    char addr[INET6_ADDRSTRLEN]{};
    auto sa = reinterpret_cast<const sockaddr*>(&sockinfo);

    ++add_count;
    fmt::print("connections: {}/{}\n", rel_count, add_count);
    while (true) {
        try {
            auto data = co_await stream.Read(200);
            if (data.empty()) {
                break;
            }
            fmt::print("Received: '{}' from '{}:{}'\n", data.data(),
                       inet_ntop(sockinfo.ss_family, GetInAddr(sa), addr, sizeof addr),
                       GetInPort(sa));
            co_await stream.Write(data);
        } catch (...) {
            break;
        }
    }
    ++rel_count;
    fmt::print("connections: {}/{}\n", rel_count, add_count);
    stream.Close();
}

Task<> echo_server() {
    auto server = co_await StartServer(handle_echo, "127.0.0.1", 9012);

    fmt::print("Serving on 127.0.0.1:9012\n");

    co_await server.ServeForever();
}

int main() {
    Run(echo_server());
    return 0;
}
