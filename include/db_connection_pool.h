#include <queue>
#include <mutex>
#include <condition_variable>
#include <pqxx/pqxx>

class DBConnectionPool {
public:
    static DBConnectionPool& getInstance() {
        static DBConnectionPool instance;
        return instance;
    }

    void initialize(const std::string& conn_str, size_t pool_size = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < pool_size; ++i) {
            connections_.push(std::make_shared<pqxx::connection>(conn_str));
        }
    }

    std::shared_ptr<pqxx::connection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        // 添加超时避免永久等待
        if (!cv_.wait_for(lock, std::chrono::seconds(5), [this] { 
            return !connections_.empty(); 
        })) {
            throw std::runtime_error("Timeout waiting for database connection");
        }
        
        auto conn = connections_.front();
        connections_.pop();
        
        // 检查连接是否有效
        if (!conn->is_open()) {
            try {
                // 创建新连接替换失效连接
                conn = createNewConnection();
            } catch (const std::exception& e) {
                spdlog::error("Failed to create new connection: {}", e.what());
                throw;
            }
        }
        return conn;
    }

    void returnConnection(std::shared_ptr<pqxx::connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->is_open()) {
            connections_.push(conn);
        } else {
            // 连接已失效，创建新连接补充池
            connections_.push(createNewConnection());
        }
        cv_.notify_one();
    }

private:
    std::shared_ptr<pqxx::connection> createNewConnection() {
        // 从配置获取连接字符串
        auto& config = ConfigManager::getInstance();
        std::string conn_str = config.getNodeDatabaseConfig("center").getConnectionString();
        return std::make_shared<pqxx::connection>(conn_str);
    }
    DBConnectionPool() = default;
    std::queue<std::shared_ptr<pqxx::connection>> connections_;
    std::mutex mutex_;
    std::condition_variable cv_;
};