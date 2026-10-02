#pragma once
#include <string>

class Motor {
private:
    int pwmChannelL;
    int pwmChannelR;
    std::string pwmBasePathL;
    std::string pwmBasePathR;
    
    bool writeSysfs(const std::string& path, const std::string& filename, const std::string& value);
public:
    Motor(int channel_l, int channel_r);
    void begin();
    void setSpeed(int speed); // -255 to 255
    void stop();
};
