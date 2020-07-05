#include "application.h"
#include "unit-test/unit-test.h"

test(TIME_01_SyncTimeInAutomaticMode) {
    set_system_mode(AUTOMATIC);
    assertEqual(System.mode(),AUTOMATIC);
    Particle.connect();
    if (!waitFor(Particle.connected, 120000)) {
        Serial.println("Timed out waiting to connect!");
        fail();
        return;
    }

    Particle.syncTime();
    if (!waitFor(Particle.syncTimeDone, 120000)) {
        Serial.println("Timed out waiting for time sync!");
        fail();
        return;
    }
    delay(4000);

    for(int x=0; x<2; x++) {
        time_t syncedLastUnix, syncedCurrentUnix;
        system_tick_t syncedCurrentMillis;
        system_tick_t syncedLastMillis = Particle.timeSyncedLast(syncedLastUnix);
        // 2018/01/01 00:00:00
        Time.setTime(1514764800);
        assertEqual(Time.now(), 1514764800);
        Particle.disconnect();
        if (!waitFor(Particle.disconnected, 120000)) {
            Serial.println("Timed out waiting to disconnect!");
            fail();
            return;
        }
        // set_system_mode(AUTOMATIC);
        // assertEqual(System.mode(),AUTOMATIC);
        delay(20000);

        Particle.connect();
        if (!waitFor(Particle.connected, 120000)) {
            Serial.println("Timed out waiting to connect!");
            fail();
            return;
        }
        // Just in case send sync time request (Electron might not send it after handshake if the session was resumed)
        if (!Particle.syncTimePending()) {
            Particle.syncTime();
        }
        if (!waitFor(Particle.syncTimeDone, 120000)) {
            Serial.println("Timed out waiting for time sync!");
            fail();
            return;
        }

        assertTrue(Time.isValid());
        assertMore(Time.year(), 2018);
        syncedCurrentMillis = Particle.timeSyncedLast(syncedCurrentUnix);
        // Serial.printlnf("sCU-sLU: %d, sCM-sLM: %d",
        //     syncedCurrentUnix-syncedLastUnix, syncedCurrentMillis-syncedLastMillis);
        assertMore(syncedCurrentMillis, syncedLastMillis);
        assertMore(syncedCurrentUnix, syncedLastUnix);
    } // for()
}

test(TIME_02_SyncTimeInManualMode) {
    for(int x=0; x<2; x++) {
        time_t syncedLastUnix, syncedCurrentUnix;
        system_tick_t syncedCurrentMillis;
        system_tick_t syncedLastMillis = Particle.timeSyncedLast(syncedLastUnix);
        // 2018/01/01 00:00:00
        Time.setTime(1514764800);
        assertEqual(Time.now(), 1514764800);
        Particle.disconnect();
        if (!waitFor(Particle.disconnected, 120000)) {
            Serial.println("Timed out waiting to disconnect!");
            fail();
            return;
        }
        set_system_mode(MANUAL);
        assertEqual(System.mode(),MANUAL);
        delay(20000);

        // Serial.println("CONNECT");
        Particle.connect();
        if (!waitFor(Particle.connected, 120000)) {
            Serial.println("Timed out waiting to connect!");
            fail();
            return;
        }

        // Just in case send sync time request (Electron might not send it after handshake if the session was resumed)
        // Serial.println("SYNC TIME");
        if (!Particle.syncTimePending()) {
            Particle.syncTime();
        }
        if (!waitFor(Particle.syncTimeDone, 120000)) {
            Serial.println("Timed out waiting for time sync!");
            fail();
            return;
        }
        assertTrue(Time.isValid());
        assertMore(Time.year(), 2018);
        syncedCurrentMillis = Particle.timeSyncedLast(syncedCurrentUnix);
        // Serial.printlnf("sCU-sLU: %d, sCM-sLM: %d",
        //     syncedCurrentUnix-syncedLastUnix, syncedCurrentMillis-syncedLastMillis);
        assertMore(syncedCurrentMillis, syncedLastMillis);
        assertMore(syncedCurrentUnix, syncedLastUnix);
    } // for()
}
