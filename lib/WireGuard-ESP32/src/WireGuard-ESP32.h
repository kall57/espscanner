// MODIFIÉ par le projet ESP SCANNER (adaptation Arduino-ESP32 3.x, clé pré-partagée, état du tunnel) — licence d origine : BSD-3-Clause, voir LICENSE.
/*
 * WireGuard implementation for ESP32 Arduino by Kenta Ida (fuga@fugafuga.org)
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include <IPAddress.h>

class WireGuard
{
private:
    bool _is_initialized = false;
public:
    bool begin(const IPAddress& localIP, const IPAddress& Subnet, const IPAddress& Gateway, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort, bool makeDefault = true, const char* presharedKey = nullptr);
    bool begin(const IPAddress& localIP, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort);
    void end();
    bool peer_up() const;   // ADAPTATION : vrai quand la poignée de main a réussi
    bool is_initialized() const { return this->_is_initialized; }
};
