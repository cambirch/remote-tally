#include <Arduino.h>

/** Set to indicate that NeoPixels are to be used instead */
extern bool isStripLights;
/** Set to indicate that true/false should be inverted for talking to a relay */
extern bool isInvertedRelay;

/** The name sent by the controller to indicate that tally set 1 should enable */
extern String tally1Name;
/** The name sent by the controller to indicate that tally set 2 should enable */
extern String tally2Name;
/** The name sent by the controller to indicate that tally set 3 should enable */
extern String tally3Name;
/** The name sent by the controller to indicate that tally set 4 should enable */
// char tally4Name[10] = "";

/** 
 * Called in the setup() method to ensure that the configuration is correctly initialized 
 * @returns true if the settings were wiped by a user pressing the button.
 * @param allowA0ToWipe when true (recommended) allows the A0 pin when set high (via button) to wipe the settings.
 */
bool initSettings(bool allowA0ToWipe);

/** Erases the eeprom settings */
void wipeSettings();

/** 
 * Loads the configuration details from storage.
 * @returns true if the configuration was present, false otherwise
 */
bool restoreConfig();

void connectToWifi();
