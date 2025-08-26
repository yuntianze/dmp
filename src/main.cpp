#include <iostream>
#include <memory>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include "common/types.hpp"

int main() {
    try {
        spdlog::info("🚀 启动 DMP 风控原型系统");
        spdlog::info("📋 目标性能: P99 ≤ 50ms, QPS ≥ 10,000");
        
        // 配置 Drogon 服务器
        drogon::app()
            .setLogPath("./logs")
            .setLogLevel(trantor::Logger::kInfo)
            .addListener("0.0.0.0", 8080)
            .setThreadNum(8)
            .enableRunAsDaemon()
            .run();
            
    } catch (const std::exception& e) {
        spdlog::error("❌ 启动失败: {}", e.what());
        return 1;
    }
    
    return 0;
}
