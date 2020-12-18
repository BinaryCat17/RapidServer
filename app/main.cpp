#include "rapid/impl.hpp"

using namespace rapid;

int main() {
    auto explorer = explorer::make(unused, Traits<explorer::Default>(), "data");
    auto v = Merge(uWS::App(), explorer);
    rapid::server::start(v, Traits<
        server::Default,
        server_callback::Default,
        socket_callback::Default,
        response::Default,
        socket::Default
    >(), 9003);
}