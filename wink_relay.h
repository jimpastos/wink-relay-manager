#pragma once

#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "TaskScheduler.hpp"
#include "linux/input.h"

#define BUTTON_0_GPIO "/sys/class/gpio/gpio8/"
#define BUTTON_1_GPIO "/sys/class/gpio/gpio7/"
#define RELAY_0_GPIO "/sys/class/gpio/gpio203/"
#define RELAY_1_GPIO "/sys/class/gpio/gpio204/"

#define SCREEN_STATE  "/sys/class/gpio/gpio30/value"
#define SCREEN_INPUT_EVENTS "/dev/input/event0"
#define AMBIENT_LIGHT_IR_INPUT_EVENTS "/dev/input/event1"
#define AMBIENT_LIGHT_INPUT_EVENTS "/dev/input/event2"
#define PROXIMITY_INPUT_EVENTS "/dev/input/event3"
#define	TEMPERATURE_DATA "/sys/bus/i2c/devices/2-0040/temp1_input"
#define HUMIDITY_DATA "/sys/bus/i2c/devices/2-0040/humidity1_input"

struct RelayCallbacks {
  virtual void buttonClicked(int button, int clicks) = 0;
  virtual void buttonHeld(int button, int clicks) = 0;
  virtual void buttonReleased(int button, int clicks) = 0;

  virtual void relayStateChanged(int relay, bool state) = 0;
  virtual void temperatureChanged(float value) = 0;
  virtual void humidityChanged(float value) = 0;
  virtual void proximityTriggered(int p) = 0;
  virtual ~RelayCallbacks() = default;
};

struct ButtonState {
  bool held = false;
  int clickCount = 0;
};

int writeFile(const char* file, const char* data, int dataLen) {
  int fd = open(file, O_WRONLY);
  if (fd) {
    return write(fd, data, dataLen);
    close(fd);
  }
  return -1;
}

class WinkRelay {
public:
  WinkRelay()
  : m_started(false), m_looper(), m_cb(nullptr), m_screenTimeout(20) {
    clearStates();
  }

  void setCallbacks(RelayCallbacks* cb) {
    m_cb = cb;
  }

  void setScreenTimeout(int sec) {
    m_screenTimeout = std::chrono::seconds(sec);
  }

  void setProximityThreshold(int t) {
    m_proximityThreshold = t;
  }

  void start(bool async) {
    using namespace std::chrono_literals;
    if (!m_started) {
      m_started = true;

      // Check Relay On/Off state and temp/humidity every 500ms
      m_scheduler.Schedule(500ms, [this] (tsc::TaskContext c) {
        checkRelayStates();
        // Temperature
        char buf[10] = {0}; // for re-use
        if (checkValue(m_temperatureFd, buf, sizeof(buf), 100, m_lastTemperature) && m_cb) {
          m_cb->temperatureChanged(m_lastTemperature/1000.0f);
        }
        // Humidity
        if (checkValue(m_humidityFd, buf, sizeof(buf), 100, m_lastHumidity) && m_cb) {
          m_cb->humidityChanged(m_lastHumidity/1000.0f);
        }
        c.Repeat();
      });

      if (async) {
        m_looper = std::thread(&WinkRelay::looperThread, this);
      } else {
        looperThread();
      }
    }
  }

  bool setRelay(int relay, bool enabled) {
    if (relay == 0 || relay == 1) {
      m_scheduler.Async([this, relay, enabled] () {
        auto fd = m_relayFds[relay];
        lseek(fd, 0, SEEK_SET);
        write(fd, enabled ? "1":"0", 1);
      });
      return true;
    }
    return false;
  }

  bool toggleRelay(int relay) {
    if (relay == 0 || relay == 1) {
      m_scheduler.Async([this, relay] () {
        char state;
        auto fd = m_relayFds[relay];
        // read state then flip
        lseek(fd, 0, SEEK_SET);
        read(fd, &state, 1);
        lseek(fd, 0, SEEK_SET);
        if (state == '0') {
          state = '1';
        } else {
          state = '0';
        }
        write(fd, &state, 1);
      });
      return true;
    }
    return false;
  }

  void setScreen(bool enabled) {
    m_scheduler.Async([this, enabled]() {
      screenPower(enabled);
    });
  }

  // Reset state in order to trigger new events
  void resetState() {
    m_scheduler.Async([this]() {
      clearStates();
    });
  }

  tsc::TaskScheduler& scheduler() {
    return m_scheduler;
  }

private:
  bool m_started;
  std::thread m_looper;
  RelayCallbacks* m_cb;
  tsc::TaskScheduler m_scheduler;
  // Config
  std::chrono::seconds m_screenTimeout;
  int m_proximityThreshold = 5000;
  // File Handles
  int m_temperatureFd;
  int m_humidityFd;
  int m_screenFd;
  int m_relayFds[2];
  // States
  ButtonState m_buttonStates[2] = {{0}, {0}};
  char m_relayStates[2];
  int m_lastTemperature;
  int m_lastHumidity;
  int m_lastInput;

  enum SchedulerGroup {
    BUTTON_0 = 0,
    BUTTON_1,
    SCREEN
  };

  void clearStates() {
    m_lastTemperature = -1;
    m_lastHumidity = -1;
    m_lastInput = -1;
    m_relayStates[0] = ' ';
    m_relayStates[1] = ' ';
  }

  void handleButtonPress(int i) {
    using namespace std::chrono_literals;
    screenPower(true);
    auto& s = m_buttonStates[i];
    // cancel all for this button (group id)
    m_scheduler.CancelGroup(i);
    s.clickCount++;
    // Using button id as Group Id
    m_scheduler.Schedule(400ms, i, [this, i, &s] (tsc::TaskContext c) {
      // no more events after 200ms => held
      s.held = true;
      if (m_cb) {
        m_cb->buttonHeld(i, s.clickCount);
      }
      c.Repeat(); // send repeated held events every 500ms
    });
  }

  void handleButtonRelease(int i) { 
    using namespace std::chrono_literals;
    screenPower(true);
    auto& s = m_buttonStates[i];
    // cancel all for this button
    m_scheduler.CancelGroup(i);
    if (s.held) {
      s.held = false;
      if (m_cb) {
        m_cb->buttonReleased(i, s.clickCount);
      }
      s.clickCount = 0;
    } else {
      // TODO Check max clicks and send immediately
      m_scheduler.Schedule(150ms, i, [this, i, &s] (tsc::TaskContext) {
        // no more events after 150ms -> clicked
        if (m_cb) {
          m_cb->buttonClicked(i, s.clickCount);
        }
        s.clickCount = 0;
      }); 
    }
  }

  void checkRelayStates() {
    char state;
    for (int i=0; i<2;++i) {
      lseek(m_relayFds[i], 0, SEEK_SET);
      if (read(m_relayFds[i], &state, 1) > 0) {
        if (m_relayStates[i] != state) {
          m_relayStates[i] = state;
          if (m_cb) {
            m_cb->relayStateChanged(i, state == '1');
          }
        }
      }
    }
  }

  bool checkValue(int fd, char* buffer, int len, int threshold, int& last) {
    int value = getInteger(fd, buffer, len);
    if (abs(value - last) > threshold) {
      last = value;
      return true;
    }
    return false;
  }

  int getInteger(int fd, char* buffer, int len) {
    lseek(fd, 0, SEEK_SET);
    if (read(fd, buffer, len) > 0) {
      return atoi(buffer);
    }
    return -1;
  }

  void screenPower(bool enabled) {
    using namespace std::chrono_literals;
    // Cancel previous schedules
    m_scheduler.CancelGroup(SCREEN);
    if (enabled) {
      write(m_screenFd, "1", 1);
      m_scheduler.Schedule(m_screenTimeout, SCREEN, [this] (tsc::TaskContext c) {
        // turn off screen
        write(m_screenFd, "0", 1);
      });
    } else {
      write(m_screenFd, "0", 1);
    }
  }

  void consumeEvents(int fd, struct input_event* event, std::function<void(struct input_event*)> cb) {
    while (read(fd, event, sizeof(struct input_event)) > 0) {
      if (cb) {
        cb(event);
      }
    }
  }

  void processScreenEvent(int fd, struct input_event* event) {
    bool trigger = false;
    consumeEvents(fd, event, [&trigger] (struct input_event* e) {
      if (e->type == EV_KEY) {
        trigger = true;
      }
    });
    if (trigger) {
      screenPower(true);
    }
  }

  void processProximityEvent(int fd, struct input_event* event) {
    int i = 0;
    uint16_t led_a = 0, led_b=0, led_c=0;
    consumeEvents(fd, event, [&i, &led_a, &led_b, &led_c] (struct input_event* e) {
      if (i == 0) { // led a
        led_a = e->value;
      } else if (i == 1) { // led b
        led_b = e->value;
      } else if (i == 2) { // led c
        led_c = e->value;
      }
      // ignore rest
      ++i;
    });

    // only using led_a for now
    if (led_a >= m_proximityThreshold) {
      screenPower(true);
      if (m_cb) {
        m_cb->proximityTriggered(led_a);
      }
    }
  }

  void processAmbientLightEvent(int fd, struct input_event* event) {
    consumeEvents(fd, event, [] (struct input_event* e) {
      if (e->type == EV_ABS) {
        // printf("Got ambient light event %d\n", e->value);
      }
      // ignore others
    });
  }

  void processAmbientLightIREvent(int fd, struct input_event* event) {
    consumeEvents(fd, event, [] (struct input_event* e) {
      if (e->type == EV_ABS) {
        // printf("Got ambient light IR event %d\n", e->value);
      }
      // ignore others
    });
  }

  void looperThread() {
    using namespace std::chrono_literals;
    // set edges to listen to both signals
    writeFile(BUTTON_0_GPIO"edge", "both", 4);
    writeFile(BUTTON_1_GPIO"edge", "both", 4);

    // setup button listeners
    const char* pollFiles[] = { BUTTON_0_GPIO"value", BUTTON_1_GPIO"value",
                                SCREEN_INPUT_EVENTS,
                                PROXIMITY_INPUT_EVENTS,
                                //AMBIENT_LIGHT_INPUT_EVENTS,
                                //AMBIENT_LIGHT_IR_INPUT_EVENTS
                              };
    int pollFileCount = sizeof(pollFiles) / sizeof(char*);
    struct pollfd fdlist[pollFileCount];

    for (int i=0;i<2; ++i) { // gpio polling
      fdlist[i].fd = open(pollFiles[i], O_RDONLY);
      fdlist[i].events = POLLPRI|POLLERR;
      fdlist[i].revents = 0;
    }

    for (int i=2;i<pollFileCount; ++i) {
      // input events
      fdlist[i].fd = open(pollFiles[i], O_RDONLY | O_NONBLOCK);
      fdlist[i].events = POLLIN;
      fdlist[i].revents = 0;
    }

    // opem files
    m_relayFds[0] = open(RELAY_0_GPIO"value", O_RDWR);
    m_relayFds[1] = open(RELAY_1_GPIO"value", O_RDWR);
    m_temperatureFd = open(TEMPERATURE_DATA, O_RDONLY);
    m_humidityFd = open(HUMIDITY_DATA, O_RDONLY);
    m_screenFd = open(SCREEN_STATE, O_RDWR);

    // read initial button data and start fresh
    char buf[2] = {0};
    for (int i=0;i<2;++i) {
      lseek(fdlist[i].fd, 0, SEEK_SET);
      read(fdlist[i].fd, buf, sizeof(buf));
    }

    // main loop
    struct input_event event; // for re-use
    int err;
    while (1) {
      // Relying on poll timeout for scheduler periodic updates
      err = poll(fdlist, pollFileCount, 50); // 50ms
      if (-1 == err) {
        perror("poll");
        return;
      }
      if (err > 0) {
        // No timeout with at least 1 update
        for (int i=0;i<pollFileCount; ++i) {
          if (i < 2) {
            if ((fdlist[i].revents & POLLPRI) == POLLPRI) {
              lseek(fdlist[i].fd, 0, SEEK_SET);
              read(fdlist[i].fd, buf, sizeof(buf));
              if (buf[0] == '0') {
                handleButtonPress(i);
              }
              if (buf[0] == '1') {
                handleButtonRelease(i);
              }
            }
          } else {
            // event data
            if ((fdlist[i].revents & POLLIN) == POLLIN) {
              if (i == 2) {
                processScreenEvent(fdlist[i].fd, &event);
              } else if (i == 3) {
                processProximityEvent(fdlist[i].fd, &event);
              } else if (i == 4) {
                processAmbientLightEvent(fdlist[i].fd, &event);
              } else if (i == 5) {
                processAmbientLightIREvent(fdlist[i].fd, &event);
              }
            }
          }
        }
      } // else time out
      m_scheduler.Update();
    }
  }
};
