/** 
 *  \file server.cpp
 *  \brief Implementation of the Server class
 */
#define MG_DISABLE_DAV_AUTH 
#define MG_ENABLE_FAKE_DAVLOCK
extern "C" {
#include "mongoose.h"
}
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <atomic>
#include <thread>
#include <fstream>
#include <regex>
#include <chrono>
#include <asio.hpp>

#define WEBSOCKET 0
#ifndef DEMO
#define DEMO 0
#define DEMO_WITH_LED_CONTROL 0
#endif
#ifndef DEMO_WITH_LED_CONTROL
#if __arm__
#define DEMO_WITH_LED_CONTROL 1
#else
#define DEMO_WITH_LED_CONTROL 0
#endif
#endif

#if DEMO
std::string distant_end;
#if DEMO_WITH_LED_CONTROL
#include "gpio.h"
GPIO *lamp{nullptr};
#endif
#endif

static std::atomic_int done{false};

/// Placeholder for web-settable settings.  Currently unused.
struct device_settings {
  char setting1[100];
  char setting2[100];
};

#if WEBSOCKET
static std::fstream diag("moocow");
#endif // WEBSOCKET

static struct device_settings s_settings{"value1", "value2"};

static void handle_save(struct mg_connection *nc, struct http_message *hm) {
  // Get form variables and store settings values
  mg_get_http_var(&hm->body, "setting1", s_settings.setting1,
                  sizeof(s_settings.setting1));
  mg_get_http_var(&hm->body, "setting2", s_settings.setting2,
                  sizeof(s_settings.setting2));

  // Send response
  mg_printf(nc, "%s", "HTTP/1.1 302 OK\r\nLocation: /\r\n\r\n");
}

static std::string replace_entities(std::string str) {
    static const std::regex space_regex("%20");
    std::string ret = std::regex_replace(str, space_regex, " ");
    std::swap(ret, str);
    return str;    
}

#if 0
// This function is to detect a partial response
static bool isIncomplete(std::string &response) {
    return response.length() < 3 || response.front() != '{' || response.back() != '}';
}
#endif

static std::string tool_call(std::string &request) {
    asio::ip::tcp::iostream stream;
    stream.expires_from_now(std::chrono::milliseconds(300));
    asio::ip::tcp::endpoint endpoint(
        asio::ip::address::from_string("127.0.0.1"), 5555);
    stream.connect(endpoint);
    stream << request;
    stream.flush();
#if 0
    std::cout << "Sending: " << request << "\n";
#endif
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string response;
    std::getline(stream, response);
#if 0
    for (std::getline(stream, response); isIncomplete(response);  ) {
        stream.flush();
        std::cout << "#";
        std::string partial;
        std::getline(stream, partial);
        response += partial;
    }
    std::cout << "Reply was: " << response << "\n";
#endif
    return response;
}

static void handle_tool_request(struct mg_connection *nc, http_message *hm) {
    constexpr std::size_t cmdsize{100};
    char cmd[cmdsize];
    auto ret = mg_get_http_var(&hm->query_string, "cmd", cmd, cmdsize);
    std::string command = replace_entities(cmd);
    if (ret > 0) {
    std::string result = tool_call(command);

  // Use chunked encoding in order to avoid calculating Content-Length
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  // Output JSON object which holds CPU usage data
  mg_printf_http_chunk(nc, result.data());

  // Send empty chunk, the end of response
  mg_send_http_chunk(nc, "", 0);
    }
}

#if DEMO
static void sendMsg(const char *msg) {
    using asio::ip::udp;

    try {
        asio::io_service io_service;
        udp::socket socket(io_service, udp::endpoint(udp::v6(), 0));
        udp::resolver resolver(io_service);
        udp::endpoint endpoint = *resolver.resolve({udp::v6(), distant_end, "4321"});
        size_t msg_len = std::strlen(msg);
        socket.send_to(asio::buffer(msg, msg_len), endpoint);
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
    }
}


static void sendOutage(const std::chrono::time_point<std::chrono::system_clock> &outageStart) {
    std::time_t tt{std::chrono::system_clock::to_time_t(outageStart)};
#if 0
    std::cout << std::put_time(std::localtime(&tt), "O:%T") << '\n';
#else
    char temp[100];
    std::strftime(temp, sizeof(temp), "O:%T", std::localtime(&tt));
    std::cout << temp << '\n';
    sendMsg(temp);
#endif
}


static void sendOutage(
        const std::chrono::time_point<std::chrono::system_clock> &outageEnd, 
        const std::chrono::time_point<std::chrono::system_clock> &outageStart)
{
    auto duration(std::chrono::duration_cast<std::chrono::seconds>(outageEnd - outageStart));
    std::time_t tt{std::chrono::system_clock::to_time_t(outageEnd)};
#if 0
    std::cout << std::put_time(std::localtime(&tt), "R:%T,D:")
       << duration.count()
        << '\n';
#else
    char temp[100];
    std::strftime(temp, sizeof(temp), "R:%T", std::localtime(&tt));
    std::cout << temp << duration.count() << '\n';
    sendMsg(temp);
#endif
}


static void handle_pin_request(struct mg_connection *nc, http_message *hm) {
    static std::chrono::time_point<std::chrono::system_clock> outageStart;
    constexpr std::size_t cmdsize{100};
    char cmd[cmdsize];
    auto ret = mg_get_http_var(&hm->query_string, "pin", cmd, cmdsize);
    std::string command = replace_entities(cmd);
    bool on = command == "on";
    if (ret > 0) {
#if DEMO_WITH_LED_CONTROL
        lamp->operator()(on);
#endif
        if (on) {
            // paradoxically, "on" means outage in this usage
            outageStart = std::chrono::system_clock::now();
            sendOutage(outageStart);
        } else {
            auto outageEnd = std::chrono::system_clock::now();
            sendOutage(outageEnd, outageStart);
        }
        mg_printf(nc, "%s", "HTTP/1.1 204 OK\r\nContent-Length: 0\r\n\r\n");
    }
}
#endif //DEMO

static void handle_get_cpu_usage(struct mg_connection *nc) {
  // Generate random value, as an example of changing CPU usage
  // Getting real CPU usage depends on the OS.
  int cpu_usage = (double) rand() / RAND_MAX * 100.0;

  // Use chunked encoding in order to avoid calculating Content-Length
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  // Output JSON object which holds CPU usage data
  mg_printf_http_chunk(nc, "{ \"result\": %d }", cpu_usage);

  // Send empty chunk, the end of response
  mg_send_http_chunk(nc, "", 0);
}

static void handle_ssi_call(struct mg_connection *nc, const char *param) {
  if (strcmp(param, "setting1") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.setting1);
  } else if (strcmp(param, "setting2") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.setting2);
  }
}

/// Wrapper class for the C-code mongoose web server.
class WebServer 
{
    const char *s_http_port;
    struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_serve_http_opts s_http_server_opts;

    static void ev_handler(mg_connection *nc, int ev, void *p) {
        http_message *hm = (http_message *)p;
        switch (ev) {
            case MG_EV_HTTP_REQUEST:
                if (mg_vcmp(&hm->uri, "/save") == 0) {
                    handle_save(nc, hm);
                } else if (mg_vcmp(&hm->uri, "/get_diag") == 0) {
                    handle_get_cpu_usage(nc);
#if DEMO
                } else if (mg_vcmp(&hm->uri, "/set") == 0) {
                    handle_pin_request(nc, hm);
#endif //DEMO
                } else if (mg_vcmp(&hm->uri, "/tool") == 0) {
                    handle_tool_request(nc, hm);
                } else {
                    // serve static content 
                    mg_serve_http(nc, hm, *(mg_serve_http_opts *)nc->user_data);
                }
                break;
            case MG_EV_SSI_CALL:
                handle_ssi_call(nc, (const char *)p);
                break;
            default:
                break;
        }
    }

public:
    WebServer(const char *webroot, const char *port = "8000") 
        : s_http_port{port}
    {
        std::memset(&s_http_server_opts, 0, sizeof(s_http_server_opts));
        mg_mgr_init(&mgr, NULL);
        nc = mg_bind(&mgr, s_http_port, ev_handler);

        // pass our options to event handler
        nc->user_data = &s_http_server_opts;

        // Set up HTTP server parameters
        mg_set_protocol_http_websocket(nc);
        s_http_server_opts.document_root = webroot;   // Serve web_root dir 
        s_http_server_opts.dav_document_root = ".";         // Allow access via WebDav
        s_http_server_opts.enable_directory_listing = "no";
    }
    void run() {
        std::cout << "Starting web server on port " << s_http_port << "\n";
#if WEBSOCKET
        while (!done) { 
            static time_t last_time;
            time_t now = time(NULL);
            mg_mgr_poll(&mgr, 1000);
            if (now - last_time > 0) {
                push_data_to_all_websocket_connections(&mgr);
                last_time = now;
            }
        }
#else // WEBSOCKET
        while (!done) { 
            mg_mgr_poll(&mgr, 1000);
        }
#endif // WEBSOCKET
    }
#if WEBSOCKET
    static void push_data_to_all_websocket_connections(struct mg_mgr *m) {
        struct mg_connection *c;
        std::string line;
        std::getline(diag, line);
        //std::cout << "Got: " << line << "\n";
        for (c = mg_next(m, NULL); c != NULL; c = mg_next(m, c)) {
            if (c->flags & MG_F_IS_WEBSOCKET) {
                mg_printf_websocket_frame(c, WEBSOCKET_OP_TEXT, "%s", line.c_str());
            }
        }
    }
#endif // WEBSOCKET

    virtual ~WebServer() {
        mg_mgr_free(&mgr);
#if WEBSOCKET
        diag.close();
#endif // WEBSOCKET
    }
};

void serve(const char *webroot) {
    WebServer server(webroot);
    server.run();
}
    
int main(int argc, char *argv[]) {
#if DEMO
    if (argc != 3) {
        std::cout << "Usage: web_server web_root_dir demo_ip\n"
            "where demo_ip may be a resolvable machine name or IPv6 address\n";
        return 1;
    }
    distant_end = argv[2];
#if DEMO_WITH_LED_CONTROL
    lamp = new GPIO{4};
#endif  //DEMO_WITH_LED_CONTROL
#else // DEMO
    if (argc != 2) {
        std::cout << "Usage: web_server web_root_dir\n";
        return 1;
    }
#endif // DEMO
    std::thread web{serve, argv[1]};
    std::string command;
    std::cout << "Enter the word \"quit\" to exit the program and shut down the server\n";
    while (!done && std::cin >> command) {
        if (command == "quit") {
            std::cout << "Shutting down web server now...\n";
            done = true;
        }
    }
    std::cout << "Joining web server thread...\n";
    web.join();
#if DEMO_WITH_LED_CONTROL
    delete lamp;
#endif 
}
