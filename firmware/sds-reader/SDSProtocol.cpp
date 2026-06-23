#include "SDSProtocol.h"

SDSProtocol::SDSProtocol(HardwareSerial *kline, uint8_t tx_pin, uint8_t dealer_pin) {
    _kline = kline;
    _tx_pin = tx_pin;
    _dealer_pin = dealer_pin;
    _connected = false;
    _dealer_mode = false;
    _use_length = false;
    _use_addr = true;
    _debug = false;
    _dbg = NULL;
    _resp_len = 0;
    _resp_data_start = 0;

    rpm = 0; tps = 0; stps = 0; gear = 0; speed = 0;
    ect = 0; iat = 0; map_kpa = 0; baro_kpa = 0;
    battery_v = 0; clutch = 0; neutral = 0;
    ignition_angle = 0; pair_valve = 0;
    for (int i = 0; i < 4; i++) injector_pw[i] = 0;
}

void SDSProtocol::enable_debug(Stream *s) {
    _dbg = s;
    _debug = true;
}

void SDSProtocol::set_dealer_mode(bool on) {
    _dealer_mode = on;
    digitalWrite(_dealer_pin, on ? HIGH : LOW);
    if (_debug) {
        _dbg->print(F("Dealer mode: "));
        _dbg->println(on ? F("ON") : F("OFF"));
    }
}

bool SDSProtocol::get_dealer_mode() {
    return _dealer_mode;
}

uint8_t SDSProtocol::calc_checksum(uint8_t *buf, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += buf[i];
    return sum;
}

void SDSProtocol::wake_kline() {
    digitalWrite(_tx_pin, HIGH);
    delay(ISO_T_IDLE);
    digitalWrite(_tx_pin, LOW);
    delay(ISO_T_INIL);
    digitalWrite(_tx_pin, HIGH);
    delay(ISO_T_WUP);
}

void SDSProtocol::send_frame(uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        _kline->write(data[i]);
        if (_debug) {
            _dbg->print(F("TX: 0x"));
            _dbg->println(data[i], HEX);
        }
        delay(ISO_T_P4_MIN);
    }
    _kline->flush();
}

bool SDSProtocol::recv_frame(uint32_t timeout_ms) {
    _resp_len = 0;
    _resp_data_start = 0;
    memset(_response, 0, ISO_MAX_DATA);

    uint32_t start = millis();
    uint8_t byte_idx = 0;
    bool header_done = false;
    uint8_t data_to_rcv = 0;
    uint8_t data_rcvd = 0;

    while (millis() - start < timeout_ms) {
        if (_kline->available()) {
            uint8_t b = _kline->read();
            _response[byte_idx] = b;

            if (_debug) {
                _dbg->print(F("RX: 0x"));
                _dbg->println(b, HEX);
            }

            if (!header_done) {
                if (byte_idx == 0) {
                    if ((b & 0xC0) != FMT_PHYSICAL) {
                        if (_debug) _dbg->println(F("Bad format byte"));
                        return false;
                    }
                    if (_use_length) {
                        data_to_rcv = b & 0x3F;
                        if (data_to_rcv == 0) data_to_rcv = 255;
                    } else if ((b & 0x3F) != 0) {
                        data_to_rcv = b & 0x3F;
                    }
                }
                if (_use_addr) {
                    if (byte_idx == 1 && b != OUR_ADDR) {
                        if (_debug) _dbg->println(F("Not addressed to us"));
                        return false;
                    }
                    if (byte_idx == 2 && b != ECU_ADDR) {
                        if (_debug) _dbg->println(F("Not from ECU"));
                        return false;
                    }
                }
                if (byte_idx == (_use_addr ? 2 : 0)) {
                    if (!_use_length && data_to_rcv == 0) {
                        data_to_rcv = b;
                        _use_length = true;
                    }
                }
                if (byte_idx == (_use_addr ? 2 : 0)) {
                    if (data_to_rcv > 0) {
                        data_to_rcv = b;
                    }
                }
                if (byte_idx == (_use_addr ? 3 : 1)) {
                    if (_use_length && data_to_rcv == 0) {
                        data_to_rcv = b;
                    }
                }
                if (byte_idx >= (_use_addr ? 3 : 1) && data_to_rcv > 0) {
                    header_done = true;
                    _resp_data_start = byte_idx + 1;
                }
            } else {
                data_rcvd++;
                if (data_rcvd >= data_to_rcv + 1) {
                    _resp_len = byte_idx + 1;
                    uint8_t cs = calc_checksum(_response, _resp_len - 1);
                    if (cs != _response[_resp_len - 1]) {
                        if (_debug) {
                            _dbg->print(F("Checksum fail: calc=0x"));
                            _dbg->print(cs, HEX);
                            _dbg->print(F(" got=0x"));
                            _dbg->println(_response[_resp_len - 1], HEX);
                        }
                        return false;
                    }
                    return true;
                }
            }
            byte_idx++;
            start = millis();
        }
    }
    if (_debug) _dbg->println(F("Response timeout"));
    return false;
}

bool SDSProtocol::exchange(uint8_t *req, uint8_t req_len, uint8_t expected_sid) {
    send_frame(req, req_len);
    delay(ISO_T_P2_MIN);
    if (!recv_frame(ISO_T_P2_MAX)) return false;
    if (_response[_resp_data_start] != expected_sid) {
        if (_debug) {
            _dbg->print(F("Unexpected SID: 0x"));
            _dbg->print(_response[_resp_data_start], HEX);
            _dbg->print(F(" expected 0x"));
            _dbg->println(expected_sid, HEX);
        }
        return false;
    }
    return true;
}

bool SDSProtocol::begin() {
    pinMode(_tx_pin, OUTPUT);
    pinMode(_dealer_pin, OUTPUT);
    digitalWrite(_dealer_pin, LOW);

    if (_debug) _dbg->println(F("Waking K-Line..."));
    wake_kline();

    _kline->begin(SDS_BAUD);
    delay(50);

    uint8_t start_req[] = {0x81};
    if (!exchange(start_req, 1, 0xC1)) {
        if (_debug) _dbg->println(F("Start comm failed"));
        return false;
    }

    if (_debug) _dbg->println(F("K-Line initialized"));
    _connected = true;
    return true;
}

void SDSProtocol::end() {
    uint8_t stop_req[] = {0x82};
    exchange(stop_req, 1, 0xC2);
    _kline->end();
    _connected = false;
    if (_debug) _dbg->println(F("Disconnected"));
}

bool SDSProtocol::is_connected() {
    return _connected;
}

bool SDSProtocol::request_sensors() {
    if (!_connected) return false;

    // Request sensor data: ReadDataByLocalIdentifier (0x21), identifier 0x08
    uint8_t req[] = {0x21, 0x08};
    if (!exchange(req, 2, 0x61)) return false;

    if (_resp_len < _resp_data_start + 48) {
        if (_debug) _dbg->println(F("Response too short"));
        return false;
    }

    uint8_t *d = &_response[_resp_data_start];

    speed = d[16];
    rpm = d[17] * 10 + d[18] / 10;
    tps = 125 * (d[19] - 55) / (256 - 55);
    if (tps > 100) tps = 100;
    map_kpa = d[20] * 4 * 0.136;
    ect = (d[21] - 48) / 1.6;
    iat = (d[22] - 48) / 1.6;
    battery_v = d[24] * 100 / 126.0;
    gear = d[26];
    stps = d[46] / 2.55;
    ignition_angle = d[47];
    clutch = d[52];
    neutral = d[53];

    injector_pw[0] = d[31] * 256 + d[32];
    injector_pw[1] = d[33] * 256 + d[34];
    injector_pw[2] = d[35] * 256 + d[36];
    injector_pw[3] = d[37] * 256 + d[38];

    baro_kpa = d[27] * 4 * 0.136;
    pair_valve = d[51];

    return true;
}

bool SDSProtocol::read_dtcs() {
    if (!_connected) return false;
    uint8_t req[] = {0x13};
    return exchange(req, 1, 0x53);
}

bool SDSProtocol::clear_dtcs() {
    if (!_connected) return false;
    uint8_t req[] = {0x14};
    return exchange(req, 1, 0x54);
}

bool SDSProtocol::keep_alive() {
    if (!_connected) return false;
    uint8_t req[] = {0x3E, 0x01};
    return exchange(req, 2, 0x7E);
}

uint8_t *SDSProtocol::get_response() {
    return _response;
}

uint8_t SDSProtocol::get_response_len() {
    return _resp_len;
}

uint8_t SDSProtocol::get_response_data_start() {
    return _resp_data_start;
}
