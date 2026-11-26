#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct TelemetrySnapshot {
    std::string mode = "NONE";
    bool dry_run = false;
    bool controls_enabled = true;
    bool cruise_active = false;
    float cruise_speed = 0.0f;

    int left_pwm = 0;
    int right_pwm = 0;
    long long enc_left = 0;
    long long enc_right = 0;

    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float l2 = 0.0f;
    float r2 = 0.0f;
    bool dpad_up = false;
    bool dpad_down = false;
    bool dpad_left = false;
    bool dpad_right = false;
    bool r1 = false;

    int ss_left_raw = 0;
    int ss_right_raw = 0;
    int ss_dir = 1;
};

class WebDebugServer {
private:
    int port = 8080;
    int listen_fd = -1;
    std::atomic<bool> running{false};
    std::thread server_thread;

    mutable std::mutex state_mutex;
    TelemetrySnapshot snapshot;

    std::function<void()> on_estop_toggle;
    std::function<void()> on_enc_reset;

    void serverLoop();
    void handleClient(int client_fd);
    std::string buildJson() const;

public:
    WebDebugServer(int listen_port,
                   std::function<void()> estop_cb,
                   std::function<void()> enc_reset_cb);
    ~WebDebugServer();

    WebDebugServer(const WebDebugServer&) = delete;
    WebDebugServer& operator=(const WebDebugServer&) = delete;

    bool start();
    void stop();
    void updateTelemetry(const TelemetrySnapshot& next);
};
