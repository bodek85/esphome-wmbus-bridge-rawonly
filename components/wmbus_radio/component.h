#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <functional>
#include <string>

#include "freertos/FreeRTOS.h"

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include "esphome/components/spi/spi.h"
// Keep component lightweight (no full wmbusmeters stack)
#include "link_mode.h"

#include "packet.h"
#include "transceiver.h"

namespace esphome {
namespace wmbus_radio {

class Radio : public Component {
public:
  void set_radio(RadioTransceiver *radio) { this->radio = radio; };
  void set_diag_topic(const std::string &topic) { this->diag_topic_ = topic; }

  void setup() override;
  void loop() override;
  void receive_frame();

  void add_frame_handler(std::function<void(Frame *)> &&callback);

protected:
  static void wakeup_receiver_task_from_isr(TaskHandle_t *arg);
  static void receiver_task(Radio *arg);

  RadioTransceiver *radio{nullptr};
  TaskHandle_t receiver_task_handle_{nullptr};
  QueueHandle_t packet_queue_{nullptr};

  std::vector<std::function<void(Frame *)>> handlers_;


  // Diagnostics counters (published every 60s if diagnostic_topic is set)
  static constexpr uint32_t DIAG_SUMMARY_INTERVAL_MS = 60000;

  enum DropBucket : uint8_t {
    DB_TOO_SHORT = 0,
    DB_DECODE_FAILED,
    DB_DLL_CRC_STRIP_FAILED,
    DB_UNKNOWN_PREAMBLE,
    DB_L_FIELD_INVALID,
    DB_UNKNOWN_LINK_MODE,
    DB_OTHER,
    DB_COUNT
  };

  uint32_t diag_truncated_{0};
  uint32_t diag_dropped_{0};
  std::array<uint32_t, DB_COUNT> diag_dropped_by_bucket_{};
  uint32_t last_diag_summary_ms_{0};

  static DropBucket bucket_for_reason_(const std::string &reason);
  void maybe_publish_diag_summary_(uint32_t now_ms);

  std::string diag_topic_{"wmbus/diag"};
};
} // namespace wmbus_radio
} // namespace esphome
