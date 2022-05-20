
#pragma once

#include "esphome.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
    namespace frigidaire {

        const uint8_t FRIGIDAIRE_TEMP_C_MIN = 16;
        const uint8_t FRIGIDAIRE_TEMP_C_MAX = 32;
        const float FRIGIDAIR_TEMP_C_STEP = 1.0f;

        enum Mode {
            MODE_AUTO = 0x7,
            COOL = 0x6,
            DRY  = 0x5,
            FAN  = 0x1,
            MODE_INVALID = 0xFF
        };

        enum SwingMode {
            SWING_ON = 0x7,
            SWING_OFF = 0x0,
            SWING_INVALID = 0xFF
        };

        enum FanSpeed {
            FAN_AUTO = 0x5,
            FAN_HIGH = 0xD,
            FAN_MID  = 0xB,
            FAN_LOW  = 0x9,
            FAN_INVALID = 0xFF,
        };

        class Payload {
            private:
                // Byte 0
                uint8_t identity   :8;
                // Byte 1
                uint8_t swing      :3;
                uint8_t temprature :5;
                // Byte 2
                uint8_t            :8;
                // Byte 3
                uint8_t            :8;
                // Byte 4
                uint8_t            :4;
                uint8_t fanSpeed   :4;
                // Byte 5
                uint8_t            :8;
                // Byte 6
                uint8_t            :5;
                uint8_t mode       :3;
                // Byte 7
                uint8_t            :8;
                // Byte 8
                uint8_t            :8;
                // Byte 9
                uint8_t            :5;
                uint8_t power      :1;
                uint8_t            :2;
                // Byte 10
                uint8_t            :8;
                // Byte 11
                uint8_t            :8;
                // Byte 12
                uint8_t sum        :8;
            public:
                Payload();

                bool isPowered();
                void setPowered(bool powered);
                uint8_t getChecksum();
                uint8_t getTempratureC();
                void setTempratureC(uint8_t temprature);
                Mode getMode();
                void setMode(Mode mode);
                SwingMode getSwingMode();
                void setSwingMode(SwingMode swing);
                FanSpeed getFanSpeed();
                void setFanSpeed(FanSpeed fanSpeed);
        };

        class FrigidareClimate: public climate_ir::ClimateIR {
            public:
                FrigidareClimate() : climate_ir::ClimateIR(
                    static_cast<float>(FRIGIDAIRE_TEMP_C_MIN),
                    static_cast<float>(FRIGIDAIRE_TEMP_C_MAX),
                    FRIGIDAIR_TEMP_C_STEP) {}

                // void control(const climate::ClimateCall &call) override;
                climate::ClimateTraits traits() override;
            protected:
                void transmit_state() override;
                bool on_receive(remote_base::RemoteReceiveData data) override;
            private:
                Payload payload;
        };
    }
}
