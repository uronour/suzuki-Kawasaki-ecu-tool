#ifndef SDS_PROTOCOL_H
#define SDS_PROTOCOL_H

#include <Arduino.h>

#define SDS_BAUD 10400
#define ISO_T_IDLE 1000
#define ISO_T_INIL 25
#define ISO_T_WUP 50
#define ISO_T_P2_MIN 50
#define ISO_T_P2_MAX 500
#define ISO_T_P3_MAX 2000
#define ISO_T_P4_MIN 5
#define ISO_T_P1 10
#define ISO_MAX_DATA 260

#define ECU_ADDR 0x12
#define OUR_ADDR 0xF1
#define FMT_PHYSICAL 0x80

class SDSProtocol {
private:
    HardwareSerial *_kline;
    uint8_t _tx_pin;
    uint8_t _dealer_pin;
    bool _connected;
    bool _dealer_mode;
    uint8_t _response[ISO_MAX_DATA];
    uint8_t _resp_len;
    uint8_t _resp_data_start;
    bool _use_length;
    bool _use_addr;
    bool _debug;
    Stream *_dbg;

    uint8_t calc_checksum(uint8_t *buf, uint8_t len);
    void wake_kline();
    void send_frame(uint8_t *data, uint8_t len);
    bool recv_frame(uint32_t timeout_ms);
    bool exchange(uint8_t *req, uint8_t req_len, uint8_t expected_sid);

public:
    SDSProtocol(HardwareSerial *kline, uint8_t tx_pin, uint8_t dealer_pin);

    void enable_debug(Stream *s);
    void set_dealer_mode(bool on);
    bool get_dealer_mode();
    bool begin();
    void end();
    bool is_connected();

    bool request_sensors();
    bool read_dtcs();
    bool clear_dtcs();
    bool keep_alive();

    uint8_t *get_response();
    uint8_t get_response_len();
    uint8_t get_response_data_start();

    // Parsed sensor values
    uint16_t rpm;
    uint8_t tps;
    uint8_t stps;
    uint8_t gear;
    uint8_t speed;
    uint8_t ect;
    uint8_t iat;
    uint16_t map_kpa;
    uint16_t baro_kpa;
    float battery_v;
    uint8_t clutch;
    uint8_t neutral;
    uint16_t injector_pw[4];
    uint8_t ignition_angle;
    uint8_t pair_valve;
};

#endif
