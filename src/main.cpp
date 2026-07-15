#include <drogon/drogon.h>

int main()
{
    drogon::app().registerHandler(
        "/",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(R"({"status":"ok","service":"url-shortener"})");
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            callback(resp);
        });

    drogon::app().addListener("0.0.0.0", 8080);
    drogon::app().run();
    return 0;
}
