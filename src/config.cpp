#include <config.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>

/** Set to indicate that NeoPixels are to be used instead */
bool isStripLights = false;
/** Set to indicate that true/false should be inverted for talking to a relay */
bool isInvertedRelay = false;

/** The name sent by the controller to indicate that tally set 1 should enable */
String tally1Name = "";
/** The name sent by the controller to indicate that tally set 2 should enable */
String tally2Name = "";
/** The name sent by the controller to indicate that tally set 3 should enable */
String tally3Name = "";
/** The name sent by the controller to indicate that tally set 4 should enable */
// char tally4Name[10] = "";

String ssid = "";
String ssid_pass = "";

/** The size of the reserved EEPROM storage size */
const int EEPROM_SIZE = 512;

/** 
 * Called in the setup() method to ensure that the configuration is correctly initialized 
 * @returns true if the settings were wiped by a user pressing the button.
 * @param allowA0ToWipe when true (recommended) allows the A0 pin when set high (via button) to wipe the settings.
 */
bool initSettings(bool allowA0ToWipe)
{
    bool isWipeSettings = false;

    EEPROM.begin(EEPROM_SIZE);

    // Check for a reset command
    if (allowA0ToWipe)
    {
        isWipeSettings = analogRead(A0) > 500;
        if (isWipeSettings)
        {
            for (int i = 0; i < 512; i++)
            {
                EEPROM.write(i, 0);
            }
            EEPROM.commit();
        }
    }

    return isWipeSettings;
}

/** Erases the eeprom settings */
void wipeSettings()
{
    for (int i = 0; i < EEPROM_SIZE; i++)
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}

String readStringFromEEPROM(int start, int length)
{
    String valueToRead = "";

    for (int i = 0; i < length; ++i)
    {
        valueToRead += char(EEPROM.read(start + i));
    }

    return valueToRead;
}

/** 
 * Loads the configuration details from storage.
 * @returns true if the configuration was present, false otherwise
 */
bool restoreConfig()
{
    Serial.println("Reading EEPROM...");

    // Check if settings have been stored (a blank EEPROM first character means no settings)
    if (EEPROM.read(0) == 0)
    {
        Serial.println("Config not found.");
        return false;
    }

    // Read SSID
    ssid = readStringFromEEPROM(0, 32);
    // Read Password
    ssid_pass = readStringFromEEPROM(32, 64);

    // Restore tally names
    tally1Name = readStringFromEEPROM(96, 10).c_str();
    tally2Name = readStringFromEEPROM(106, 10).c_str();
    tally3Name = readStringFromEEPROM(116, 10).c_str();
    // tally4Name = readStringFromEEPROM(126, 10).c_str();

    // Read flags
    byte flags = EEPROM.read(136);

    isStripLights = flags & 1;
    isInvertedRelay = flags & 2;

    return true;
}

void connectToWifi()
{
    WiFi.begin(ssid.c_str(), ssid_pass.c_str());
}
