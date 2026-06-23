#include <Arduino.h>
#include <KWP2000.h>

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial bike(2);
#define TX_PIN 17
#define DEALER_PIN 4
#else
#define bike Serial
#define TX_PIN 1
#define DEALER_PIN 4
#endif

#define debug Serial

KWP2000 ECU(&bike, TX_PIN, SUZUKI);

void setup()
{
    debug.begin(115200);
    while (!debug) { ; }

    debug.println(F("Suzuki SDS Reader v1.0"));
    debug.println(F("2004 GSX-R1000 K-Line Interface"));
    debug.println(F("--------------------------------"));

    ECU.enableDebug(&debug, DEBUG_LEVEL_DEFAULT, 115200);
    ECU.enableDealerMode(DEALER_PIN);
    ECU.use_metric();

    debug.println(F("\nCommands: i=init, s=sensors, d=dealer, t=DTCs, p=status, c=close, ?=help"));
}

void loop()
{
    if (debug.available() > 0)
    {
        char in = debug.read();
        switch (in)
        {
        case 'i':
        case 'I':
            debug.println(F("\n--- Initializing K-Line ---"));
            while (ECU.initKline() == 0)
            {
                debug.print(F("."));
                delay(100);
            }
            if (ECU.getStatus())
            {
                debug.println(F("Connected!"));
            }
            else
            {
                debug.println(F("Failed to connect!"));
            }
            break;

        case 'd':
        case 'D':
            ECU.setDealerMode(!ECU.getDealerMode());
            debug.print(F("Dealer mode: "));
            debug.println(ECU.getDealerMode() ? F("ON") : F("OFF"));
            break;

        case 's':
        case 'S':
            if (!ECU.getStatus())
            {
                debug.println(F("Not connected! Use 'i' first."));
                break;
            }
            debug.println(F("\n--- Sensor Data ---"));
            ECU.requestSensorsData();
            ECU.printSensorsData();
            printCustomSensors();
            break;

        case 't':
        case 'T':
            if (!ECU.getStatus())
            {
                debug.println(F("Not connected! Use 'i' first."));
                break;
            }
            debug.println(F("\n--- Reading DTCs ---"));
            ECU.readTroubleCodes(READ_TOTAL);
            break;

        case 'c':
        case 'C':
            if (!ECU.getStatus())
            {
                debug.println(F("Already disconnected."));
                break;
            }
            debug.println(F("\n--- Closing connection ---"));
            while (ECU.stopKline() == 0) { ; }
            debug.println(F("Disconnected."));
            break;

        case 'p':
        case 'P':
            ECU.printStatus();
            break;

        case 'r':
        case 'R':
            debug.println(F("\n--- Raw response ---"));
            ECU.printLastResponse();
            break;

        case '?':
        case 'h':
        case 'H':
            printHelp();
            break;

        case '\n':
        case '\r':
            break;

        default:
            debug.print(F("Unknown: "));
            debug.println(in);
            debug.println(F("Type ? for help"));
            break;
        }
    }

    ECU.keepAlive();
    ECU.printStatus(5000);
}

void printCustomSensors()
{
    debug.println(F("--- Custom Decode ---"));

    float rpm = ECU.getRPM();
    uint8_t tps = ECU.getTPS();
    uint8_t gear = ECU.getGPS();
    uint8_t speed = ECU.getSPEED();
    uint8_t ect = ECU.getECT();
    uint8_t iat = ECU.getIAT();
    float volt = ECU.getVOLT();

    debug.print(F("Engine: "));
    debug.print(rpm);
    debug.println(F(" RPM"));

    debug.print(F("Speed: "));
    debug.print(speed);
    debug.println(F(" km/h"));

    debug.print(F("Gear: "));
    debug.println(gear);

    debug.print(F("TPS: "));
    debug.print(tps);
    debug.println(F("%"));

    debug.print(F("Coolant: "));
    debug.print(ect);
    debug.println(F(" C"));

    debug.print(F("Intake Air: "));
    debug.print(iat);
    debug.println(F(" C"));

    debug.print(F("Battery: "));
    debug.print(volt);
    debug.println(F(" V"));

    debug.print(F("Fuel Inj 1: "));
    debug.print(ECU.getInjectorPW(1));
    debug.println(F(" ms"));

    debug.print(F("Ignition Angle: "));
    debug.print(ECU.getIgnitionAngle());
    debug.println(F(" deg"));

    debug.print(F("Clutch: "));
    debug.println(ECU.getCLUTCH() ? F("Engaged") : F("Disengaged"));

    debug.print(F("Secondary TPS: "));
    debug.print(ECU.getSTPS());
    debug.println(F("%"));

    debug.print(F("MAP: "));
    debug.print(ECU.getIAP());
    debug.println(F(" kPa"));

    debug.println(F("------------------------\n"));
}

void printHelp()
{
    debug.println(F("\nCommands:"));
    debug.println(F("  i - Initialize K-Line & connect to ECU"));
    debug.println(F("  s - Request all sensor data"));
    debug.println(F("  d - Toggle dealer mode"));
    debug.println(F("  t - Read diagnostic trouble codes"));
    debug.println(F("  c - Close connection"));
    debug.println(F("  p - Print connection status"));
    debug.println(F("  r - Print raw last response"));
    debug.println(F("  ? - This help"));
}
