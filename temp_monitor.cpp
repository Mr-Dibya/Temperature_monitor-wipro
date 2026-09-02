#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "tempsensor_ioctl.h"

enum class State {
    NORMAL,
    WARNING,
    CRITICAL
};

static const char* state_name(State s)
{
    switch (s) {
        case State::NORMAL:   return "NORMAL";
        case State::WARNING:  return "WARNING";
        case State::CRITICAL: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

static State classify(double temp)
{
    if (temp >= 80.0) return State::CRITICAL;
    if (temp >= 50.0) return State::WARNING;
    return State::NORMAL;
}

static std::string timestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

static double read_temperature(int fd)
{
    (void)fd;
    int dev_fd = open("/dev/tempsensor", O_RDONLY);
    if (dev_fd < 0) {
        perror("open /dev/tempsensor");
        return -1.0;
    }
    char buf[16] = {0};
    ssize_t n = read(dev_fd, buf, sizeof(buf) - 1);
    close(dev_fd);
    if (n <= 0) {
        return -1.0;
    }
    buf[n] = '\0';
    return std::atof(buf);
}

int main(int argc, char *argv[])
{
    // Disable stdout buffering so output is flushed immediately to file
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    const char *dev_path = "/dev/tempsensor";
    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/tempsensor (is the module loaded? are you root?)");
        return 1;
    }

    if (argc == 2 && std::strcmp(argv[1], "--reset") == 0) {
        if (ioctl(fd, TEMP_IOC_RESET) < 0) {
            perror("ioctl RESET");
            close(fd);
            return 1;
        }
        printf("[%s] Sensor reset to baseline.\n", timestamp().c_str());
        close(fd);
        return 0;
    }

    if (argc == 3 && std::strcmp(argv[1], "--drift") == 0) {
        int drift = std::atoi(argv[2]);
        if (ioctl(fd, TEMP_IOC_SET_DRIFT, &drift) < 0) {
            perror("ioctl SET_DRIFT");
            close(fd);
            return 1;
        }
        printf("[%s] Drift set to %.1f C per reading.\n",
               timestamp().c_str(), drift / 10.0);
    }

    printf("[%s] Monitoring started. Press Ctrl+C to stop.\n",
           timestamp().c_str());

    State current_state = State::NORMAL;
    printf("[%s] Initial state: %s\n", timestamp().c_str(), state_name(current_state));

    while (true) {
        double temp = read_temperature(fd);
        if (temp < 0) {
            fprintf(stderr, "[%s] Failed to read sensor, retrying...\n",
                    timestamp().c_str());
        } else {
            State new_state = classify(temp);

            if (new_state != current_state) {
                printf("[%s] TRANSITION: %s -> %s  (temp = %.1f C)\n",
                       timestamp().c_str(),
                       state_name(current_state), state_name(new_state),
                       temp);
                current_state = new_state;
            } else {
                printf("[%s] temp = %.1f C  (state: %s)\n",
                       timestamp().c_str(), temp, state_name(current_state));
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close(fd);
    return 0;
}
