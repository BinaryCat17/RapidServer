#include "App.h"
#include "rapid/file.hpp"
#include "rapid/sockets.hpp"
#include "rapid/user.hpp"

using namespace std;

struct UserData {
    string login = "undefined";
};

int main() {
    rapid::CacheExplorer explorer("data");

    uWS::App().get("/*", [&explorer](uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        auto url = req->getUrl();
        cout << "New URL! " << url << endl;
        try {
            res->write(explorer.file(url.substr(1)));
        } catch(exception const& e) {
            res->write("ERROR: Cannot read file");
            cerr << e.what() << endl;
        }
        res->end();
    }).ws<UserData>("/*", {
        .open = [](uWS::WebSocket<false, true> *ws) {
           cout << "Noviy chelick podkluchilsa! " << ws->getRemoteAddress() << endl;
        },
        .message = rapid::response<UserData>([&explorer](UserData*, string_view message) {
            if(message == "get xml") {
                cout << "Sending xml" << endl;
                return "xml " + string(explorer.file("data.xml"));
            } else {
                return "Undefined request"s;
            }
        })
    }).listen("0.0.0.0", 9003, [](auto *listenSocket) {
        if (listenSocket) {
            cout << "Listening for connections..." << endl;
        }
    }).run();

    cout << "Shoot! We failed to listen and the App fell through, exiting now!" << endl;
}