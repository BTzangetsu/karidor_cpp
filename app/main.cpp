#include "sockets/events.hpp"

bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    // Compare la fin de 'str' avec 'suffix'
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        auto page = crow::mustache::load_text("templates/index.html");
        return crow::response(page);
    });

    CROW_ROUTE(app, "/game")([](){
        auto page = crow::mustache::load_text("templates/game.html");
        return crow::response(page);
    });

    CROW_ROUTE(app, "/static/<path>")([](const std::string& path){
        auto content = crow::mustache::load_text("static/" + path);
        crow::response res(content);
        if (ends_with(path, ".css")) res.set_header("Content-Type", "text/css");
        if (ends_with(path, ".js"))  res.set_header("Content-Type", "application/javascript");
        return res;
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([](crow::websocket::connection& conn) {
        })
        .onclose([](crow::websocket::connection& conn, const std::string& reason) {
            handle_disconnect(conn);
        })
        .onmessage([](crow::websocket::connection& conn,
                      const std::string& raw, bool is_binary) {

            crow::json::rvalue json = crow::json::load(raw);
            if (!json) {
                crow::json::wvalue err;
                err["message"] = "Invalid JSON";
                emit_to(conn, "error", std::move(err));
                return;
            }

            std::string evt         = json["event"].s();
            crow::json::rvalue data = json["data"];

            if      (evt == "create_room")  handle_create_room(conn, data);
            else if (evt == "join_room")    handle_join_room(conn, data);
            else if (evt == "play_move")    handle_play_move(conn, data);
            else if (evt == "play_wall")    handle_play_wall(conn, data);
            else if (evt == "restart_game") handle_restart_game(conn, data);
            else if (evt == "rejoin_game")  handle_rejoin_game(conn, data);
            else {
                crow::json::wvalue res;
                res["message"] = "Unknown event: " + evt;
                emit_to(conn, "error", std::move(res));
            }
        });

    app.port(5000).multithreaded().run();
}