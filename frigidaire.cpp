
#include "frigidaire.h"
#include <cstdint>
#include <array>
#include <cstring>
#include <algorithm>
#include <optional>

namespace esphome {
    namespace frigidaire {
        static const char* const TAG = "frigidaire.climate";

        const uint16_t HEADER_MARK = 8968;
        const uint16_t HEADER_SPACE = 4425;

        const uint16_t BIT_MARK = 600;
        const uint16_t ONE_SPACE = 1650;
        const uint16_t ZERO_SPACE = 500;
        const uint16_t GAP_SPACE = HEADER_MARK;

        const size_t messageLength = sizeof(Payload);

        uint8_t calculateChecksum(const std::array<uint8_t, messageLength> & raw) {
            uint8_t calculatedChecksum = 0x0;
            for (std::array<uint8_t, messageLength>::const_iterator byte_iterator = raw.cbegin(), end = raw.cend() - 1; byte_iterator != end; byte_iterator += 1) {
                calculatedChecksum += *byte_iterator;
            }

            return calculatedChecksum;
        }
        
        climate::ClimateTraits FrigidareClimate::traits() {
            // The capabilities of the climate device
            auto traits = climate::ClimateTraits();
            traits.set_supports_current_temperature(true);
            traits.set_supported_modes({
                climate::CLIMATE_MODE_OFF,
                climate::CLIMATE_MODE_COOL,
                climate::CLIMATE_MODE_FAN_ONLY,
                climate::CLIMATE_MODE_DRY,
                climate::CLIMATE_MODE_AUTO
            });
            traits.set_supported_fan_modes({
                climate::ClimateFanMode::CLIMATE_FAN_AUTO,
                climate::ClimateFanMode::CLIMATE_FAN_LOW,
                climate::ClimateFanMode::CLIMATE_FAN_MEDIUM,
                climate::ClimateFanMode::CLIMATE_FAN_HIGH
            });
            traits.set_supported_swing_modes({
                climate::ClimateSwingMode::CLIMATE_SWING_OFF,
                climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL
            });

            traits.set_visual_min_temperature(16.0f);
            traits.set_visual_max_temperature(32.0f);
            traits.set_visual_temperature_step(1.0f);

            return traits;
        }

        void FrigidareClimate::transmit_state() {
            switch (this->mode) {
                case climate::CLIMATE_MODE_OFF:
                    // Just turn it off.
                    this->payload.setPowered(false);
                    break;
                case climate::CLIMATE_MODE_AUTO:
                    this->payload.setPowered(true); // You have to turn it on as well.
                    this->payload.setMode(Mode::MODE_AUTO);
                    break;
                case climate::CLIMATE_MODE_COOL:
                    this->payload.setPowered(true); // You have to turn it on as well.
                    this->payload.setMode(Mode::COOL);
                    break;
                case climate::CLIMATE_MODE_DRY:
                    this->payload.setPowered(true); // You have to turn it on as well.
                    this->payload.setMode(Mode::DRY);
                    break;
                case climate::CLIMATE_MODE_FAN_ONLY:
                    this->payload.setPowered(true); // You have to turn it on as well.
                    this->payload.setMode(Mode::FAN);
                    break;
                default:
                    ESP_LOGW(TAG, "Invalid mode requested: %02x", this->mode);
                    break;
            }

            if (this->fan_mode.has_value()) {
                switch (*this->fan_mode) {
                    case climate::CLIMATE_FAN_AUTO:
                        this->payload.setFanSpeed(FanSpeed::FAN_AUTO);
                        break;
                    case climate::CLIMATE_FAN_HIGH:
                        this->payload.setFanSpeed(FanSpeed::FAN_HIGH);
                        break;
                    case climate::CLIMATE_FAN_MEDIUM:
                        this->payload.setFanSpeed(FanSpeed::FAN_MID);
                        break;
                    case climate::CLIMATE_FAN_LOW:
                        this->payload.setFanSpeed(FanSpeed::FAN_LOW);
                        break;
                    default:
                        ESP_LOGW(TAG, "Invalid fan mode requested: %02x", this->fan_mode);
                        break;
                }
            } else {
                ESP_LOGW(TAG, "Fan mode unavailable.");
            }

            switch (this->swing_mode) {
                case climate::CLIMATE_SWING_VERTICAL:
                    this->payload.setSwingMode(SwingMode::SWING_ON);
                    break;
                case climate::CLIMATE_SWING_OFF:
                    this->payload.setSwingMode(SwingMode::SWING_OFF);
                    break;
                default:
                    ESP_LOGW(TAG, "Invalid swing mode requested: %02x", this->swing_mode);
                    break;
            }

            payload.setTempratureC(this->target_temperature);

            // Convert the payload to a buffer.
            std::array<uint8_t, messageLength> raw;
            std::memcpy(&raw.front(), &this->payload, raw.size());

            // Calculate the checksum.
            uint8_t calculatedChecksum = calculateChecksum(raw);
            raw.back() = calculatedChecksum;

            // TODO this is hard coded for testing.
            // raw = {0xc3, 0x4f, 0xe0, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x05, 0xb7}; // Auto mode.
            // raw = { 0xc3, 0x67, 0xe0, 0x00, 0xa0, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x05, 0xef }; // Cool mode.

            // Potentally useful for debug and troubleshooting.
            std::string stringified;
            for (uint8_t byte : raw) {
                stringified += format_hex(byte);
                stringified += "|";
            }
            ESP_LOGD(TAG, "RAW: %s", stringified.c_str());

            auto transmit = this->transmitter_->transmit();
            remote_base::RemoteTransmitData *data = transmit.get_data();

            data->reset();
            data->set_carrier_frequency(38000);
            data->reserve(8 * messageLength + 3);

            // Grab their attention with a precicely timed flash, then pause.
            data->item(HEADER_MARK, HEADER_SPACE);

            // Encode the bits.
            for (const uint8_t & byte : raw) {
                ESP_LOGD(TAG, "BYTE: %02x", byte);
                for (uint8_t bit = 0; bit < 8; bit += 1) {
                    if (((byte >> bit) & 0x01) != 0x00) {
                        data->item(BIT_MARK, ONE_SPACE);
                    } else {
                        data->item(BIT_MARK, ZERO_SPACE);
                    }
                }
            }

            // We need that extra bit mark so the receiver recognizes the end of a transmission.
            data->item(BIT_MARK, GAP_SPACE);

            // And transmit!
            // for (int i = 0; i < 5; i += 1) {
                transmit.perform();
            // }
        }

        bool FrigidareClimate::on_receive(remote_base::RemoteReceiveData data) {
            if (data.expect_item(HEADER_MARK, HEADER_SPACE)) {
                // That timing is right to be our controller.
                // That's not a guarentee it's our controller though.

                std::array<uint8_t, messageLength> raw;
                raw.fill(0); // Clear all the bits. It's safer this way.

                // Read the bits.
                for (uint8_t & byte : raw) {
                    for (uint8_t bit = 0; bit < 8; bit += 1) {
                        if (data.expect_item(BIT_MARK, ZERO_SPACE)) {
                            // It's a zero.
                            // We don't actually need to clear the bit since I cleared all of them earlier.
                            // byte &= ~(1 << bit);
                        } else if (data.expect_item(BIT_MARK, ONE_SPACE)) {
                            // It's a one.
                            byte |= 1 << bit;
                        } else {
                            // It's... not meant for us I guess?
                            // Give up on this one.
                            ESP_LOGW(TAG, "MESSAGE TOO SHORT.");
                            return false;
                        }
                    }
                    ESP_LOGD(TAG, "BYTE: %02x", byte);
                }

                // Potentally useful for debug and troubleshooting.
                std::string stringified;
                for (uint8_t byte : raw) {
                    stringified += format_hex(byte);
                    stringified += "|";
                }
                ESP_LOGD(TAG, "RAW: %s", stringified.c_str());

                // Copy it into a struct.
                Payload payload;
                std::memcpy(&payload, &raw.front(), raw.size());

                if (payload.getIdentity() == 0xc3) {
                    // Okay, so we had enough bits worth of data.
                    // Now we have to validate that data.

                    // Validate the mode.
                    if (payload.getMode() != Mode::MODE_INVALID) {
                        // Validate the swing.
                        if (payload.getSwingMode() != SwingMode::SWING_INVALID) {
                            // Validate the fan speed.
                            if (payload.getFanSpeed() != FanSpeed::FAN_INVALID) {
                                // Validate the checksum.
                                uint8_t calculatedChecksum = calculateChecksum(raw);

                                if (calculatedChecksum == payload.getChecksum()) {
                                    ESP_LOGD(TAG, "Checksum passed.");

                                    // Everything is valid!
                                    // Apply the state!

                                    // Powered is special since the controller doesn't consider it a mode.
                                    if (payload.isPowered()) {
                                        // It's on, so we need to get the mode.
                                        switch (payload.getMode()) {
                                            case Mode::MODE_AUTO:
                                                this->mode = climate::CLIMATE_MODE_AUTO;
                                                break;
                                            case Mode::COOL:
                                                this->mode = climate::CLIMATE_MODE_COOL;
                                                break;
                                            case Mode::DRY:
                                                this->mode = climate::CLIMATE_MODE_DRY;
                                                break;
                                            case Mode::FAN:
                                                this->mode = climate::CLIMATE_MODE_FAN_ONLY;
                                                break;
                                            default:
                                                // Should be impossible at this point.
                                                ESP_LOGW(TAG, "Impossible mode: %02x", payload.getMode());
                                                break;
                                        }
                                    } else {
                                        // It's off.
                                        this->mode = climate::CLIMATE_MODE_OFF;
                                    }

                                    switch (payload.getFanSpeed()) {
                                        case FanSpeed::FAN_AUTO:
                                            this->fan_mode = climate::CLIMATE_FAN_AUTO;
                                            break;
                                        case FanSpeed::FAN_HIGH:
                                            this->fan_mode = climate::CLIMATE_FAN_HIGH;
                                            break;
                                        case FanSpeed::FAN_MID:
                                            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
                                            break;
                                        case FanSpeed::FAN_LOW:
                                            this->fan_mode = climate::CLIMATE_FAN_LOW;
                                            break;
                                        default:
                                            // Should be impossible at this point.
                                            ESP_LOGW(TAG, "Impossible fan speed: %02x", payload.getFanSpeed());
                                            break;
                                    }

                                    switch (payload.getSwingMode()) {
                                        case SwingMode::SWING_ON:
                                            this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                                            break;
                                        case SwingMode::SWING_OFF:
                                            this->swing_mode = climate::CLIMATE_SWING_OFF;
                                            break;
                                        default:
                                            // SHould be impossible at this point.
                                            ESP_LOGW(TAG, "Impossible swing mode: %02x", payload.getSwingMode());
                                            break;
                                    }

                                    // Only change the temprature if we're in auto or cooling mode.
                                    switch (payload.getMode()) {
                                        case Mode::MODE_AUTO:
                                        case Mode::COOL:
                                            this->target_temperature = payload.getTempratureC();
                                            break;
                                        default:
                                            break;
                                    }

                                    this->payload = payload;

                                    // And make that state known to the world (probably home assistant).
                                    this->publish_state();

                                    return true;
                                } else {
                                    // Bad checksum.
                                    // Either corrupted or wasn't actually a message for us.
                                    ESP_LOGD(TAG, "Checksum failed. Expected: %02x Got: %02x", calculatedChecksum, payload.getChecksum());
                                }
                            } else {
                                ESP_LOGW(TAG, "Fan speed is invalid.");
                            }
                        } else {
                            ESP_LOGW(TAG, "Swing Mode is invalid.");
                        }
                    } else {
                        ESP_LOGW(TAG, "Mode is invalid.");
                    }
                } else {
                    ESP_LOGW(TAG, "Magic number did not match.");
                }
            } else {
                ESP_LOGW(TAG, "HEADER INVALID.");
            }

            return false;
        }

        Payload::Payload() {
            // It seems the controller defaults to 0xFF for a lot of defaults, so that's what we'll do to.
            std::memset(this, 0x00, sizeof(*this));

            // Set some sane defaults, just in case something doesn't get set.
            this->setSwingMode(SwingMode::SWING_OFF);
            this->setTempratureC(32);
            this->setFanSpeed(FanSpeed::FAN_LOW);
            this->setMode(Mode::MODE_AUTO);
            this->setPowered(false);

            // Some kind of magic identification number the AC unit expects to see.
            this->identity = 0xc3;
            
            this->sum = 0;
        }

        uint8_t Payload::getIdentity() {
            return this->identity;
        }

        bool Payload::isPowered() {
            return this->power == 1;
        }

        void Payload::setPowered(bool powered) {
            this->power = powered ? 1:0;
        }

        uint8_t Payload::getChecksum() {
            return this->sum;
        }

        uint8_t Payload::getTempratureC() {
            return 8 + this->temprature;
        }

        void Payload::setTempratureC(uint8_t temprature) {
            this->temprature = std::min(std::max(temprature, FRIGIDAIRE_TEMP_C_MIN), FRIGIDAIRE_TEMP_C_MAX) - 8;
        }

        Mode Payload::getMode() {
            switch (this->mode) {
                case Mode::MODE_AUTO:
                case Mode::COOL:
                case Mode::DRY:
                case Mode::FAN:
                    return static_cast<Mode>(this->mode);
                default:
                    return Mode::MODE_INVALID;
            }
        }

        void Payload::setMode(Mode mode) {
            this->mode = mode;
        }

        SwingMode Payload::getSwingMode() {
            switch (this->swing) {
                case SwingMode::SWING_ON:
                case SwingMode::SWING_OFF:
                    return static_cast<SwingMode>(this->swing);
                default:
                    return SwingMode::SWING_INVALID;
            }
        }

        void Payload::setSwingMode(SwingMode swing) {
            this->swing = swing;
        }

        FanSpeed Payload::getFanSpeed() {
            switch (this->fanSpeed) {
                case FanSpeed::FAN_AUTO:
                case FanSpeed::FAN_HIGH:
                case FanSpeed::FAN_MID:
                case FanSpeed::FAN_LOW:
                    return static_cast<FanSpeed>(this->fanSpeed);
                default:
                    return FanSpeed::FAN_INVALID;
            }
        }

        void Payload::setFanSpeed(FanSpeed fanSpeed) {
            this->fanSpeed = fanSpeed;
        }
    }
}