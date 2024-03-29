#include <Arduino.h>
#include <NimBLEDevice.h>

#define bleServerName "LYWSD03MMC"

NimBLEScan *pBLEScan;
// NimBLEAddress DEVICE_MAC("a4:c1:38:c3:75:22");
static NimBLEUUID serviceUuid("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");
static NimBLEUUID charUuid("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");

NimBLEAddress *pServerAddress;
NimBLERemoteCharacteristic *thermometerCharacteristic;

std::string tempHumiString;
boolean newTempHumi = false;
boolean doConnect = false;
boolean connected = false;

float convertVoltageToPercentage(float voltage, float maxVoltage)
{
    // Ensure that the voltage is within valid bounds
    if (voltage < 0)
    {
        voltage = 0;
    }
    else if (voltage > maxVoltage)
    {
        voltage = maxVoltage;
    }

    // Calculate the percentage
    float percentage = (voltage / maxVoltage) * 100.0;

    return percentage;
}

static void thermometerNotifyCallback(NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
                                      uint8_t *pData, size_t length, bool isNotify)
{
    if (length < 4)
    {
        Serial.println("Error: Insufficient data length");
        return;
    }

    // Combine the first two bytes into a 16-bit value
    uint16_t tempValue = (pData[1] << 8) | pData[0];
    // Divide temperature by 100
    float temperature = static_cast<float>(tempValue) / 100.0;

    // Extract humidity value
    uint16_t humidityValue = pData[2];

    // Combine the last two bytes into a 16-bit value
    uint16_t voltageValue = (pData[4] << 8) | pData[3];
    // Divide battery voltage by 100
    float batteryVoltage = static_cast<float>(voltageValue) / 100.0;

    // Convert battery voltage to percentage
    float batteryPercentage = convertVoltageToPercentage(batteryVoltage, 3.3);

    // Print the values
    Serial.println();
    Serial.printf("Temperature: %.2f°C\n", temperature);
    Serial.printf("Humidity: %d%%\n", humidityValue);
    Serial.printf("Battery: %.2f%% (%.2fv)\n", batteryPercentage, batteryVoltage);
}

void setNotifyInterval(NimBLERemoteCharacteristic *characteristic, uint16_t intervalMinutes)
{
    // Get the CCCD descriptor
    NimBLERemoteDescriptor *pCCCD = characteristic->getDescriptor(NimBLEUUID((uint16_t)0x2902));

    // Check if the CCCD descriptor is found
    if (pCCCD != nullptr)
    {
        // Define the value to write to the CCCD to set the notify interval
        uint8_t notifyIntervalValue[] = {(uint8_t)(intervalMinutes & 0xFF), (uint8_t)((intervalMinutes >> 8) & 0xFF)};

        // Write the value to the CCCD without expecting a response
        pCCCD->writeValue(notifyIntervalValue, sizeof(notifyIntervalValue), false);
    }
    else
    {
        Serial.println("CCCD descriptor not found");
    }
}

bool connectToServer(BLEAddress pAddress)
{
    NimBLEClient *pClient = NimBLEDevice::createClient();

    if (!pClient->connect(pAddress))
    {
        Serial.println("Failed to connect to server");
        return false;
    }

    NimBLERemoteService *pRemoteService = pClient->getService(serviceUuid);
    if (pRemoteService == nullptr)
    {
        Serial.println("Failed to find service UUID");
        return false;
    }

    thermometerCharacteristic = pRemoteService->getCharacteristic(charUuid);
    if (thermometerCharacteristic == nullptr)
    {
        Serial.println("Failed to find characteristic UUID");
        return false;
    }

    setNotifyInterval(thermometerCharacteristic, 3); // Set notify interval to 3 minutes

    if (thermometerCharacteristic->canNotify())
        thermometerCharacteristic->subscribe(true, thermometerNotifyCallback);

    return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *advertisedDevice)
    {
        if (advertisedDevice->getName() == bleServerName)
        {
            pBLEScan->stop();
            pBLEScan->clearResults();
            advertisedDevice->getScan()->stop();
            pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
            doConnect = true;
            Serial.println("Device found. Connecting!");
        }
    }
};

void setup()
{
    Serial.begin(115200);
    delay(3000);
    NimBLEDevice::init("");
    // NimBLEDevice::whiteListAdd(DEVICE_MAC);
    Serial.println("Scanning...");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(30);
}

void loop()
{
    if (doConnect == true)
    {
        if (connectToServer(*pServerAddress))
        {
            Serial.println("Connected to the BLE Server.");
            connected = true;
        }
        else
        {
            Serial.println("Failed to connect to the server.");
        }
        doConnect = false;
    }
    delay(1000);
}
