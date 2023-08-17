/*
 * candle-dribbler - ESP32 Zigbee light controller
 * Copyright 2023  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <driver/gpio.h>
#include <led_strip.h>
#include <sdkconfig.h>

#include <atomic>
#include <bitset>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "log.h"
#include "thread.h"

namespace nutt {

class Device;

IRAM_ATTR void ui_network_join_interrupt_handler(void *arg);

namespace ui {

struct RGBColour {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} __attribute__((packed));

namespace colour {

static constexpr const RGBColour OFF = {0, 0, 0};
static constexpr const RGBColour RED = {255, 0, 0};
static constexpr const RGBColour ORANGE = {255, 96, 0};
static constexpr const RGBColour YELLOW = {255, 255, 0};
static constexpr const RGBColour GREEN = {0, 255, 0};
static constexpr const RGBColour CYAN = {0, 255, 255};
static constexpr const RGBColour BLUE = {0, 0, 255};
static constexpr const RGBColour MAGENTA = {255, 0, 255};
static constexpr const RGBColour WHITE = {255, 255, 255};

} // namespace colour

struct LEDState {
	RGBColour colour;
	unsigned long duration_ms;

	uint64_t remaining_us{duration_ms * 1000UL};
};

struct LEDSequence {
	unsigned long duration_ms;
	std::vector<LEDState> states;

	uint64_t remaining_us{duration_ms * 1000UL};
};

/* Ordered by priority (high to low) */
enum class Event {
	NETWORK_UNCONFIGURED_FAILED,
	NETWORK_CONFIGURED_FAILED,
	NETWORK_ERROR,
	NETWORK_CONFIGURED_CONNECTING,
	NETWORK_CONFIGURED_DISCONNECTED,
	NETWORK_UNCONFIGURED_CONNECTING,
	NETWORK_UNCONFIGURED_DISCONNECTED,
	OTA_UPDATE_ERROR,
	IDENTIFY,
	LIGHT_SWITCHED_REMOTE,
	LIGHT_SWITCHED_LOCAL,
	OTA_UPDATE_OK,
	NETWORK_CONNECT,
	NETWORK_CONNECTED,
	IDLE,
};

enum class NetworkState {
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	FAILED,
};

} // namespace ui

class UserInterface: public WakeupThread {
	friend void ui_network_join_interrupt_handler(void *arg);

public:
	UserInterface(Logging &logging, gpio_num_t network_join_pin, bool active_low);
	~UserInterface() = delete;

	static constexpr const char *TAG = "nutt.UI";

	void attach(Device &device);
	void start();

	void network_state(bool configured, ui::NetworkState state);
	void network_error();
	void identify(uint16_t seconds);
	void light_switched(bool local);
	void ota_update(bool ok);

private:
	static constexpr const uint64_t DEBOUNCE_PRESS_US = 100 * 1000;
	static constexpr const uint64_t DEBOUNCE_RELEASE_US = 1 * 1000 * 1000;
	static constexpr const uint8_t LED_LEVEL = CONFIG_NUTT_UI_LED_BRIGHTNESS;
	static const std::unordered_map<ui::Event,ui::LEDSequence> led_sequences_;

	inline int button_active() const { return button_active_low_ ? 0 : 1; }
	inline int button_inactive() const { return button_active_low_ ? 1 : 0; }

	unsigned long run_tasks() override;
	IRAM_ATTR void network_join_interrupt_handler();
	void uart_handler();

	void start_event(ui::Event event);
	void restart_event(ui::Event event);
	bool event_active(ui::Event event);
	void stop_event(ui::Event event);
	void stop_events(std::initializer_list<ui::Event> events);
	void set_led(ui::RGBColour colour);
	unsigned long update_led();

	Logging &logging_;
	led_strip_handle_t led_strip_{nullptr};
	Device *device_{nullptr};

	const gpio_num_t button_pin_;
	const bool button_active_low_;

	int button_change_state_{button_inactive()};
	uint64_t button_change_us_{0};
	int button_state_{button_inactive()};
	unsigned long button_change_count_{0};
	std::atomic<unsigned long> button_change_count_irq_{0};

	uint64_t render_time_us_{0};
	ui::Event render_event_{ui::Event::IDLE};

	std::mutex mutex_;
	std::bitset<static_cast<unsigned long>(ui::Event::IDLE) + 1> active_events_;
	std::unordered_map<ui::Event,ui::LEDSequence> active_sequence_;
};

} // namespace nutt
