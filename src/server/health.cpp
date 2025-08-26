#include <drogon/drogon.h>
#include <chrono>

using namespace drogon;

namespace dmp {

class HealthController : public HttpController<HealthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::health_check, "/health", Get);
    ADD_METHOD_TO(HealthController::ready_check, "/ready", Get);
    METHOD_LIST_END
    
    void health_check(const HttpRequestPtr& req,
                     std::function<void (const HttpResponsePtr &)>&& callback) {
        Json::Value json;
        json["status"] = "healthy";
        json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        json["version"] = "1.0.0";
        
        auto resp = HttpResponse::newHttpJsonResponse(json);
        callback(resp);
    }
    
    void ready_check(const HttpRequestPtr& req,
                    std::function<void (const HttpResponsePtr &)>&& callback) {
        // TODO: 检查依赖服务状态
        Json::Value json;
        json["status"] = "ready";
        Json::Value deps;
        deps["database"] = "connected";
        deps["cache"] = "available";
        deps["models"] = "loaded";
        json["dependencies"] = deps;
        
        auto resp = HttpResponse::newHttpJsonResponse(json);
        callback(resp);
    }
};

} // namespace dmp
