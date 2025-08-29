#include <iostream>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <map>
#include <string>

namespace ModernEmbedded {

// --- Logger ---
class Logger {
public:
    enum Level { INFO, WARN, ERROR };
    static void log(Level lvl, const std::string& msg) {
        std::string prefix;
        switch(lvl) {
            case INFO: prefix = "[INFO] "; break;
            case WARN: prefix = "[WARN] "; break;
            case ERROR: prefix = "[ERROR] "; break;
        }
        std::cout << prefix << msg << std::endl;
    }
};

// --- Event System ---
struct Event {
    std::string type;
    std::string payload;
};

class EventBus {
    std::map<std::string, std::vector<std::function<void(const Event&)>>> handlers;
    std::mutex mtx;
public:
    void registerHandler(const std::string& type, std::function<void(const Event&)> handler) {
        std::lock_guard<std::mutex> lock(mtx);
        handlers[type].push_back(handler);
    }
    void emit(const Event& evt) {
        std::lock_guard<std::mutex> lock(mtx);
        if(handlers.count(evt.type)) {
            for(auto& h : handlers[evt.type]) h(evt);
        }
    }
};

// --- Base Component ---
class Component {
public:
    virtual void init() = 0;
    virtual void update() = 0;
    virtual ~Component() {}
};

// --- Sensor HAL Interface ---
class ISensor : public Component {
public:
    virtual int read() = 0;
};

// --- Actuator HAL Interface ---
class IActuator : public Component {
public:
    virtual void write(int value) = 0;
};

// --- Example Temperature Sensor ---
class TemperatureSensor : public ISensor {
public:
    void init() override { Logger::log(Logger::INFO, "TemperatureSensor initialized"); }
    void update() override { Logger::log(Logger::INFO, "TemperatureSensor value: " + std::to_string(read())); }
    int read() override { return 20 + rand()%10; } // Simulated
};

// --- Example LED Actuator ---
class LED : public IActuator {
public:
    void init() override { Logger::log(Logger::INFO, "LED initialized"); }
    void update() override { write(rand()%100); }
    void write(int value) override { Logger::log(Logger::INFO, "LED set to " + std::to_string(value)); }
};

// --- Task Scheduler ---
class Scheduler {
    struct Task {
        std::function<void()> func;
        int intervalMs;
        std::chrono::steady_clock::time_point nextRun;
    };
    std::vector<Task> tasks;
public:
    void addTask(std::function<void()> f, int intervalMs) {
        tasks.push_back({f, intervalMs, std::chrono::steady_clock::now()});
    }
    void run(int cycles = 10) {
        for(int i=0;i<cycles;i++) {
            auto now = std::chrono::steady_clock::now();
            for(auto& t : tasks) {
                if(now >= t.nextRun) {
                    t.func();
                    t.nextRun = now + std::chrono::milliseconds(t.intervalMs);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
};

// --- Controller ---
class Controller {
    std::vector<std::shared_ptr<Component>> components;
    Scheduler scheduler;
public:
    void addComponent(std::shared_ptr<Component> c, int intervalMs=1000) {
        c->init();
        components.push_back(c);
        scheduler.addTask([c](){ c->update(); }, intervalMs);
    }
    void run(int cycles=10) { scheduler.run(cycles); }
};

} // namespace ModernEmbedded

// --- Main ---
int main() {
    using namespace ModernEmbedded;

    Controller system;

    // Shared EventBus
    auto bus = std::make_shared<EventBus>();
    bus->registerHandler("ALERT", [](const Event& e){
        Logger::log(Logger::WARN, "ALERT event received: " + e.payload);
    });

    // Add components
    system.addComponent(std::make_shared<TemperatureSensor>(), 500);
    system.addComponent(std::make_shared<LED>(), 700);

    // Emit a test event asynchronously
    std::thread([bus](){
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        bus->emit({"ALERT", "High temperature detected"});
    }).detach();

    // Run system scheduler loop
    system.run(10); // 10 cycles

    return 0;
}
