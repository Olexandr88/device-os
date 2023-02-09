/*
 * Copyright (c) 2023 Particle Industries, Inc.  All rights reserved.
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

#include "application.h"
#include "unit-test/unit-test.h"

static void startWatchdog(const WatchdogConfiguration& config) {
    WatchdogInfo info;
    assertEqual(0, Watchdog.init(config));
    assertEqual(0, Watchdog.start());
    assertEqual(0, Watchdog.getInfo(info));
    assertTrue(info.state() == WatchdogState::STARTED);
}

static void checkState(WatchdogState state) {
    WatchdogInfo info;
    assertEqual(0, Watchdog.getInfo(info));
    assertTrue(info.state() == state);
}

test(WATCHDOG_01_capabilities) {
    WatchdogInfo info;
    assertEqual(0, Watchdog.getInfo(info));
#if HAL_PLATFORM_NRF52840
    assertTrue(info.mandatoryCapabilities() == WatchdogCap::RESET);
    assertTrue(info.capabilities() == (WatchdogCap::NOTIFY | WatchdogCap::SLEEP_RUNNING | WatchdogCap::DEBUG_RUNNING));
#elif HAL_PLATFORM_RTL872X
    assertTrue(info.mandatoryCapabilities() == WatchdogCap::DEBUG_RUNNING);
    assertTrue(info.capabilities() == (WatchdogCap::RESET | WatchdogCap::NOTIFY_ONLY |
                                       WatchdogCap::RECONFIGURABLE | WatchdogCap::STOPPABLE));
#else
#error "Unsupported platform"
#endif //
}

test(WATCHDOG_02_default_1) {
    WatchdogInfo info;

    assertEqual(0, Watchdog.init(WatchdogConfiguration().timeout(5s)));
    checkState(WatchdogState::CONFIGURED);

    assertEqual(0, Watchdog.start());
    assertEqual(0, Watchdog.getInfo(info));
    assertEqual(info.configuration().timeout(), 5000);
    assertTrue(info.state() == WatchdogState::STARTED);
    assertTrue(Watchdog.started());

    delay(3000);
    Watchdog.refresh();
    delay(3000);
    Watchdog.refresh();

    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 5000));
}

test(WATCHDOG_02_default_2) {
    assertTrue(System.resetReason() == RESET_REASON_WATCHDOG);
    checkState(WatchdogState::DISABLED);
}

#if HAL_PLATFORM_RTL872X || (PLATFORM_ID == PLATFORM_TRACKER)
test(WATCHDOG_03_stopped_in_hibernate_mode_1) {
    startWatchdog(WatchdogConfiguration().timeout(5s));
    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 10000));
    System.sleep(SystemSleepConfiguration().mode(SystemSleepMode::HIBERNATE).duration(10s));
}

test(WATCHDOG_03_stopped_in_hibernate_mode_2) {
    assertTrue(System.resetReason() == RESET_REASON_POWER_MANAGEMENT);
    checkState(WatchdogState::DISABLED);
}
#endif

test(WATCHDOG_04_continue_running_after_waking_up_from_none_hibernate_mode_1) {
    startWatchdog(WatchdogConfiguration().timeout(5s).capabilities(WatchdogCap::RESET)); // SLEEP_RUNNING is cleared
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 1000));
    System.sleep(SystemSleepConfiguration().mode(SystemSleepMode::STOP).duration(7s));
    checkState(WatchdogState::STARTED);
    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 5000));
}

test(WATCHDOG_04_continue_running_after_waking_up_from_none_hibernate_mode_2) {
    assertTrue(System.resetReason() == RESET_REASON_WATCHDOG);
    checkState(WatchdogState::DISABLED);
}

test(WATCHDOG_05_system_reset_will_disable_watchdog_1) {
    startWatchdog(WatchdogConfiguration().timeout(5s));
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 1000));
    System.reset();
}

test(WATCHDOG_05_system_reset_will_disable_watchdog_2) {
    assertTrue(System.resetReason() == RESET_REASON_USER);
    checkState(WatchdogState::DISABLED);
}

#if HAL_PLATFORM_NRF52840

test(WATCHDOG_06_default_running_in_none_hibernate_mode_1) {
    startWatchdog(WatchdogConfiguration().timeout(5s));
    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 5000));
    System.sleep(SystemSleepConfiguration().mode(SystemSleepMode::STOP).duration(10s));
}

test(WATCHDOG_06_default_running_in_none_hibernate_mode_2) {
    assertTrue(System.resetReason() == RESET_REASON_WATCHDOG);
    checkState(WatchdogState::DISABLED);
}

static retained uint32_t magick = 0;

test(WATCHDOG_07_notify_1) {
    magick = 0;
    Watchdog.onExpired([&](){
        magick = 0xdeadbeef;
    });
    startWatchdog(WatchdogConfiguration().timeout(5s).capabilities(WatchdogCap::NOTIFY));
    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 5000));
}

test(WATCHDOG_07_notify_2) {
    assertTrue(System.resetReason() == RESET_REASON_WATCHDOG);
    assertEqual(magick, 0xdeadbeef);
}

#endif // HAL_PLATFORM_NRF52840

#if HAL_PLATFORM_RTL872X

system_tick_t now = 0;

test(WATCHDOG_06_interrupt_mode) {
    system_tick_t then = millis();
    Watchdog.onExpired([&](){
        now = millis();
    });
    startWatchdog(WatchdogConfiguration().timeout(5s).capabilities(WatchdogCap::NOTIFY_ONLY));

    assertTrue(waitFor([]{ return (now != 0); }, 6000));
    auto elapsed = now - then;
    assertLessOrEqual(elapsed, 5500);
    assertMoreOrEqual(elapsed, 4500);
    checkState(WatchdogState::STARTED);

    assertEqual(0, Watchdog.stop());
    checkState(WatchdogState::STOPPED);
}

test(WATCHDOG_07_stopable) {
    startWatchdog(WatchdogConfiguration().timeout(5s));
    delay(1s);
    assertEqual(0, Watchdog.stop());
    checkState(WatchdogState::STOPPED);
}

test(WATCHDOG_08_reconfigurable) {
    system_tick_t then = millis();
    startWatchdog(WatchdogConfiguration().timeout(5s));
    delay(4s);
    startWatchdog(WatchdogConfiguration().timeout(10s));
    delay(5s);
    // Notify about a pending reset
    assertEqual(0, pushMailbox(MailboxEntry().type(MailboxEntry::Type::RESET_PENDING), 5000));
}

#endif // HAL_PLATFORM_RTL872X
