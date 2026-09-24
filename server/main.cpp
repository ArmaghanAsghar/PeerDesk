#include "host_server.hpp"

#include <csignal>
#include <iostream>

namespace {
peerdesk::HostServer* g_server = nullptr;
void on_sig(int) {
    if (g_server) g_server->request_stop();
}
}  // namespace

int main(int argc, char** argv) {

    
    peerdesk::HostServer::Config cfg;


    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") {
            cfg.port = static_cast<uint16_t>(std::stoi(need("--port")));
        } else if (a == "--bind") {
            cfg.bind = need("--bind");
        } else if (a == "--data-dir") {
            cfg.data_dir = need("--data-dir");
        } else if (a == "--user") {
            cfg.bootstrap_user = need("--user");
        } else if (a == "--password") {
            cfg.bootstrap_password = need("--password");
        } else if (a == "--synthetic") {
            cfg.synthetic = true;
        } else if (a == "--no-inject") {
            cfg.inject = false;
        } else if (a == "--fps") {
            cfg.fps = std::stoi(need("--fps"));
        } else if (a == "--help" || a == "-h") {
            std::cout <<
                "peerdesk-server — Ubuntu host for PeerDesk (J0)\n"
                "  --port N          listen port (default 4473)\n"
                "  --bind ADDR       default 0.0.0.0\n"
                "  --data-dir PATH   hashes + self-signed TLS material\n"
                "  --user NAME       create this login if the store is empty (default jordan)\n"
                "  --password STR    password for that login (default peerdesk)\n"
                "  --synthetic       draw a host canvas instead of X11 (tests / Wayland)\n"
                "  --no-inject       do not open XTEST\n";
            return 0;
        } else {
            std::cerr << "Unknown flag: " << a << "\n";
            return 2;
        }
    }

    peerdesk::HostServer server(cfg);
    g_server = &server;
    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    std::string err;
    if (!server.setup(err)) {
        std::cerr << "peerdesk-server: " << err << "\n";
        return 1;
    }
    std::cout << "peerdesk-server: listening on " << cfg.bind << ":" << server.port()
              << (cfg.synthetic ? " (synthetic display)" : " (X11 capture)") << "\n";
    std::cout << "peerdesk-server: login '" << cfg.bootstrap_user
              << "' (password hashed on disk, never stored in plaintext)\n";
    server.run();
    std::cout << "peerdesk-server: stopped (still a clean exit; process was not required to die "
                 "on client disconnect)\n";
    return 0;
}
