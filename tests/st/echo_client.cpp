#include <cutecoro/cutecoro.hpp>

using namespace cutecoro;
using namespace std::chrono_literals;

Task<> tcp_echo_client(std::string_view message) {
    auto stream = co_await OpenConnection("127.0.0.1", 8888);

    fmt::print("Send: '{}'\n", message);
    co_await stream.Write(Stream::Buffer(message.begin(), message.end() + 1 /* plus '\0' */));

    auto data = co_await WaitFor(stream.Read(100), 300ms);
    fmt::print("Received: '{}'\n", data.data());

    fmt::print("Close the connection\n");
    stream.Close();
}

int main() {
    Run(tcp_echo_client("hello world!"));
    return 0;
}
