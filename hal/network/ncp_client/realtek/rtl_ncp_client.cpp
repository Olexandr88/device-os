/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#define NO_STATIC_ASSERT
#include "logging.h"
LOG_SOURCE_CATEGORY("ncp.rltk.client");

#include "rtl_ncp_client.h"

#include "gpio_hal.h"
#include "timer_hal.h"
#include "delay_hal.h"

#include "check.h"

#include <cstdlib>

#include "wifi_conf.h"
#include "wifi_constants.h"
#include "lwip_rltk.h"

#undef ON
#undef OFF
extern "C" {
#include "hal_platform_rtl.h"
#include "rtl8721dlp_sysreg.h"
#include "rtl8721dhp_sysreg.h"
#include "rtl8721dhp_rcc.h"
#include "rtl8721d.h"
} // extern "C"

#include "spark_wiring_vector.h"
#include "rtl_system_error.h"

extern "C" void rtw_efuse_boot_write(void);

namespace particle {

namespace {

int rtlSecurityToWifiSecurity(rtw_security_t rtlSec) {
    switch (rtlSec) {
    case RTW_SECURITY_OPEN: {
        return (int)WifiSecurity::NONE;
    }
    case RTW_SECURITY_WEP_PSK: {
        return (int)WifiSecurity::WEP;
    }
    case RTW_SECURITY_WPA_TKIP_PSK:
    case RTW_SECURITY_WPA_AES_PSK:
    case RTW_SECURITY_WPA_MIXED_PSK: {
        return (int)WifiSecurity::WPA_PSK;
    }
    case RTW_SECURITY_WPA2_AES_PSK:
    case RTW_SECURITY_WPA2_TKIP_PSK:
    case RTW_SECURITY_WPA2_MIXED_PSK: {
        return (int)WifiSecurity::WPA2_PSK;
    }
    case RTW_SECURITY_WPA_WPA2_TKIP_PSK:
    case RTW_SECURITY_WPA_WPA2_AES_PSK:
    case RTW_SECURITY_WPA_WPA2_MIXED_PSK: {
        return (int)WifiSecurity::WPA_WPA2_PSK;
    }
    }
    return SYSTEM_ERROR_NOT_SUPPORTED;
}

rtw_security_t wifiSecurityToRtlSecurity(WifiSecurity sec) {
    switch (sec) {
    case WifiSecurity::NONE: {
        return RTW_SECURITY_OPEN;
    }
    case WifiSecurity::WEP: {
        return RTW_SECURITY_WEP_PSK;
    }
    case WifiSecurity::WPA_PSK: {
        // FIXME
        return RTW_SECURITY_WPA_MIXED_PSK;
    }
    case WifiSecurity::WPA2_PSK: {
        // FIXME
        return RTW_SECURITY_WPA2_MIXED_PSK;
    }
    case WifiSecurity::WPA_WPA2_PSK: {
        // FIXME
        return RTW_SECURITY_WPA_WPA2_MIXED_PSK;
    }
    }
    return RTW_SECURITY_UNKNOWN;
}

} // unnamed

RealtekNcpClient::RealtekNcpClient() :
        ncpState_(NcpState::OFF),
        prevNcpState_(NcpState::OFF),
        connState_(NcpConnectionState::DISCONNECTED) {
}

RealtekNcpClient::~RealtekNcpClient() {
    destroy();
}

int RealtekNcpClient::init(const NcpClientConfig& conf) {
    conf_ = conf;
    ncpState_ = NcpState::OFF;
    prevNcpState_ = NcpState::OFF;
    connState_ = NcpConnectionState::DISCONNECTED;
    pwrState_ = NcpPowerState::OFF;
    // We know for a fact that ESP32 is off on boot because we've initialized ESPEN pin to output 0
    ncpPowerState(NcpPowerState::OFF);
    return SYSTEM_ERROR_NONE;
}

void RealtekNcpClient::destroy() {
    if (ncpState_ != NcpState::OFF) {
        ncpState_ = NcpState::OFF;
        rltkOff();
    }
}

int RealtekNcpClient::on() {
    const NcpClientLock lock(this);
    if (ncpState_ == NcpState::DISABLED) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    if (ncpState_ == NcpState::ON) {
        return SYSTEM_ERROR_NONE;
    }
    ncpPowerState(NcpPowerState::TRANSIENT_ON);
    CHECK(rltkOn());
    ncpState(NcpState::ON);
    wifi_reg_event_handler(WIFI_EVENT_DISCONNECT, [](char* buf, int buf_len, int flags, void* userdata) -> void {
        LOG(INFO, "disconnected");
        RealtekNcpClient* client = (RealtekNcpClient*)userdata;
        client->connectionState(NcpConnectionState::DISCONNECTED);
    }, (void*)this);
    return SYSTEM_ERROR_NONE;
}

int RealtekNcpClient::off() {
    const NcpClientLock lock(this);
    if (ncpState_ == NcpState::DISABLED) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    ncpPowerState(NcpPowerState::TRANSIENT_OFF);
    rltkOff();
    ncpState(NcpState::OFF);
    ncpPowerState(NcpPowerState::OFF);
    return SYSTEM_ERROR_NONE;
}

int RealtekNcpClient::enable() {
    const NcpClientLock lock(this);
    if (ncpState_ != NcpState::DISABLED) {
        return SYSTEM_ERROR_NONE;
    }
    ncpState_ = prevNcpState_;
    off();
    return SYSTEM_ERROR_NONE;
}

void RealtekNcpClient::disable() {
    // This method is used to unblock the network interface thread, so we're not trying to acquire
    // the client lock here
    const NcpState state = ncpState_;
    if (state == NcpState::DISABLED) {
        return;
    }
    prevNcpState_ = state;
    ncpState_ = NcpState::DISABLED;
}

NcpState RealtekNcpClient::ncpState() {
    return ncpState_;
}

NcpPowerState RealtekNcpClient::ncpPowerState() {
    return pwrState_;
}

int RealtekNcpClient::disconnect() {
    const NcpClientLock lock(this);
    if (ncpState_ == NcpState::DISABLED) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    if (connState_ == NcpConnectionState::DISCONNECTED) {
        return SYSTEM_ERROR_NONE;
    }
    wifi_disconnect();
    connectionState(NcpConnectionState::DISCONNECTED);
    return SYSTEM_ERROR_NONE;
}

NcpConnectionState RealtekNcpClient::connectionState() {
    return connState_;
}

int RealtekNcpClient::connect(const char* ssid, const MacAddress& bssid, WifiSecurity sec, const WifiCredentials& cred) {
    const NcpClientLock lock(this);

    CHECK_TRUE(connState_ == NcpConnectionState::DISCONNECTED, SYSTEM_ERROR_INVALID_STATE);
    char mac[32] = {};
    wifi_get_mac_address(mac);
    int rtlError = RTW_ERROR;
    for (int i = 0; i < 3; i++) {
        LOG(INFO, "Try to connect to ssid: %s, mac: %s", ssid, mac);
        rtlError = wifi_connect((char*)ssid, wifiSecurityToRtlSecurity(sec), (char*)cred.password(), strlen(ssid), strlen(cred.password()), -1, nullptr);
        if (rtlError == RTW_SUCCESS) {
            break;
        }
        HAL_Delay_Milliseconds(1000);
    }

    if (rtlError == RTW_SUCCESS) {
        connectionState(NcpConnectionState::CONNECTED);
    }
    return rtl_error_to_system(rtlError);
}

int RealtekNcpClient::getNetworkInfo(WifiNetworkInfo* info) {
    int raw_rssi = 0;
    int rtlError = wifi_get_rssi(&raw_rssi);

    if (RTW_SUCCESS == rtlError) {
        info->rssi(raw_rssi);
    }
    return rtl_error_to_system(rtlError);
}

int RealtekNcpClient::scan(WifiScanCallback callback, void* data) {
    struct Context {
        WifiScanCallback callback = nullptr;
        void* data = nullptr;
        volatile uint32_t done = 0;
        Vector<WifiScanResult> results;
    };
    Context ctx;
    ctx.callback = callback;
    ctx.data = data;
    int rtlError = wifi_scan_networks([](rtw_scan_handler_result_t* malloced_scan_result) -> rtw_result_t {
        Context* ctx = (Context*)malloced_scan_result->user_data;
        if (malloced_scan_result->scan_complete != RTW_TRUE) {
            rtw_scan_result_t* record = &malloced_scan_result->ap_details;
            record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */
#ifdef DEBUG_BUILD
            LOG(INFO, "AP");
            LOG(INFO, "%s\t ", ( record->bss_type == RTW_BSS_TYPE_ADHOC ) ? "Adhoc" : "Infra" );
            LOG(INFO, MAC_FMT, MAC_ARG(record->BSSID.octet) );
            LOG(INFO, " %d\t ", record->signal_strength );
            LOG(INFO, " %d\t  ", record->channel );
            LOG(INFO, " %d\t  ", record->wps_type );
            LOG(INFO, "%s\t\t ", ( record->security == RTW_SECURITY_OPEN ) ? "Open" :
                                            ( record->security == RTW_SECURITY_WEP_PSK ) ? "WEP" :
                                            ( record->security == RTW_SECURITY_WPA_TKIP_PSK ) ? "WPA TKIP" :
                                            ( record->security == RTW_SECURITY_WPA_AES_PSK ) ? "WPA AES" :
                                            ( record->security == RTW_SECURITY_WPA_MIXED_PSK ) ? "WPA Mixed" :
                                            ( record->security == RTW_SECURITY_WPA2_AES_PSK ) ? "WPA2 AES" :
                                            ( record->security == RTW_SECURITY_WPA2_TKIP_PSK ) ? "WPA2 TKIP" :
                                            ( record->security == RTW_SECURITY_WPA2_MIXED_PSK ) ? "WPA2 Mixed" :
                                            ( record->security == RTW_SECURITY_WPA_WPA2_TKIP_PSK) ? "WPA/WPA2 TKIP" :
                                            ( record->security == RTW_SECURITY_WPA_WPA2_AES_PSK) ? "WPA/WPA2 AES" :
                                            ( record->security == RTW_SECURITY_WPA_WPA2_MIXED_PSK) ? "WPA/WPA2 Mixed" :
            #ifdef CONFIG_SAE_SUPPORT
                                            ( record->security == RTW_SECURITY_WPA3_AES_PSK) ? "WP3-SAE AES" :
            #endif
                                            "Unknown" );

            LOG(INFO, " %s ", record->SSID.val );
            LOG(INFO, "\r\n" );
#endif // DEBUG_BUILD
            MacAddress bssid = INVALID_MAC_ADDRESS;
            memcpy(bssid.data, record->BSSID.octet, sizeof(bssid.data));

            int sec = rtlSecurityToWifiSecurity(record->security);
            if (sec >= 0) {
                auto result = WifiScanResult().ssid((char*)record->SSID.val).bssid(bssid).security((WifiSecurity)sec).channel(record->channel)
                        .rssi(record->signal_strength);
                ctx->results.append(std::move(result));
            }
            // ctx->callback(std::move(result), ctx->data);
        } else {
            ctx->done = 1;
        }
        return RTW_SUCCESS;
    }, (void*)&ctx);
    while (!ctx.done) {
        HAL_Delay_Milliseconds(100);
    }
    for (int i = 0; i < ctx.results.size(); i++) {
        callback(ctx.results[i], data);
    }
    return rtl_error_to_system(rtlError);
}

int RealtekNcpClient::getMacAddress(MacAddress* addr) {
    char mac[6*2 + 5 + 1] = {};
    wifi_get_mac_address(mac);
    CHECK_TRUE(macAddressFromString(addr, mac), SYSTEM_ERROR_UNKNOWN);
    return SYSTEM_ERROR_NONE;
}


void RealtekNcpClient::ncpState(NcpState state) {
    if (ncpState_ == NcpState::DISABLED) {
        return;
    }
    if (state == NcpState::OFF) {
        connectionState(NcpConnectionState::DISCONNECTED);
    }

    if (ncpState_ == state) {
        return;
    }
    ncpState_ = state;
    LOG(TRACE, "NCP state changed: %d", (int)ncpState_);

    const auto handler = conf_.eventHandler();
    if (handler) {
        NcpStateChangedEvent event = {};
        event.type = NcpEvent::NCP_STATE_CHANGED;
        event.state = ncpState_;
        handler(event, conf_.eventHandlerData());
    }
}

void RealtekNcpClient::ncpPowerState(NcpPowerState state) {
    if (pwrState_ == state) {
        return;
    }
    pwrState_ = state;
    const auto handler = conf_.eventHandler();
    if (handler) {
        NcpPowerStateChangedEvent event = {};
        event.type = NcpEvent::POWER_STATE_CHANGED;
        event.state = pwrState_;
        handler(event, conf_.eventHandlerData());
    }
}

void RealtekNcpClient::connectionState(NcpConnectionState state) {
    if (ncpState_ == NcpState::DISABLED) {
        return;
    }
    if (connState_ == state) {
        return;
    }
    LOG(TRACE, "NCP connection state changed: %d", (int)state);
    connState_ = state;
    const auto handler = conf_.eventHandler();
    if (handler) {
        NcpConnectionStateChangedEvent event = {};
        event.type = NcpEvent::CONNECTION_STATE_CHANGED;
        event.state = connState_;
        handler(event, conf_.eventHandlerData());
    }
}

int RealtekNcpClient::rltkOff() {
    LOG(INFO, "rltkOff");
    wifi_off();
    LOG(INFO, "rltkOff done");
    ncpPowerState(NcpPowerState::OFF);
    return SYSTEM_ERROR_NONE;
}


int RealtekNcpClient::rltkOn() {
    rtw_efuse_boot_write();
    RCC_PeriphClockCmd(APBPeriph_WL, APBPeriph_WL_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_GDMA0, APBPeriph_GDMA0_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_LCDC, APBPeriph_LCDC_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_I2S0, APBPeriph_I2S0_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_SECURITY_ENGINE, APBPeriph_SEC_ENG_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_LXBUS, APBPeriph_LXBUS_CLOCK, ENABLE);
    wifi_on(RTW_MODE_STA);
    ncpPowerState(NcpPowerState::ON);
    return SYSTEM_ERROR_NONE;
}

int RealtekNcpClient::getFirmwareVersionString(char* buf, size_t size) {
    return SYSTEM_ERROR_NONE;
}
int RealtekNcpClient::getFirmwareModuleVersion(uint16_t* ver) {
    *ver = 1;
    return SYSTEM_ERROR_NONE;
}
int RealtekNcpClient::updateFirmware(InputStream* file, size_t size) {
    return SYSTEM_ERROR_NONE;
}

int RealtekNcpClient::dataChannelWrite(int id, const uint8_t* data, size_t size) {
    return 0;
}

int RealtekNcpClient::dataChannelFlowControl(bool state) {
    return SYSTEM_ERROR_NONE;
}
void RealtekNcpClient::processEvents() {
}
int RealtekNcpClient::checkParser() {
    return SYSTEM_ERROR_NONE;
}
AtParser* RealtekNcpClient::atParser() {
    return nullptr;
}

} // particle
