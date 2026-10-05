#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LED_PIN 26
#define SENSOR_DO 27
#define SENSOR_AO 34
#define RELAY_PIN 25

#define SENSOR_DATA_READY BIT0

LiquidCrystal_I2C lcd(0x27, 16, 2);

QueueHandle_t controlQueue;
QueueHandle_t displayQueue;

SemaphoreHandle_t lcdMutex;
SemaphoreHandle_t serialMutex;

EventGroupHandle_t sensorEvents;

struct SensorData {
  int digitalValue;
  int analogValue;
};

// =====================================================
// SENSOR TASK
// =====================================================
void sensorTask(void *parameter) {

  SensorData data;

  while (1) {

    data.digitalValue = digitalRead(SENSOR_DO);
    data.analogValue = analogRead(SENSOR_AO);

    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

      Serial.print("SENSOR | DO: ");
      Serial.print(data.digitalValue);

      Serial.print(" | AO: ");
      Serial.println(data.analogValue);

      xSemaphoreGive(serialMutex);
    }

    xQueueSend(controlQueue, &data, portMAX_DELAY);

    xQueueSend(displayQueue, &data, portMAX_DELAY);

    xEventGroupSetBits(
      sensorEvents,
      SENSOR_DATA_READY
    );

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// =====================================================
// CONTROL TASK
// =====================================================
void controlTask(void *parameter) {

  SensorData receivedData;

  while (1) {

    if (xQueueReceive(
          controlQueue,
          &receivedData,
          portMAX_DELAY) == pdTRUE) {

      // LED control
      if (receivedData.digitalValue == 0) {
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }

      // Relay control
      if (receivedData.digitalValue == 0) {
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        digitalWrite(RELAY_PIN, LOW);
      }

      // Serial status
      if (xSemaphoreTake(
            serialMutex,
            pdMS_TO_TICKS(100)) == pdTRUE) {

        Serial.print("CONTROL | Sensor DO: ");
        Serial.print(receivedData.digitalValue);

        Serial.print(" | LED: ");

        if (receivedData.digitalValue == 0) {
          Serial.println("ON");
        } else {
          Serial.println("OFF");
        }

        xSemaphoreGive(serialMutex);
      }
    }
  }
}

// =====================================================
// LCD TASK
// =====================================================
void lcdTask(void *parameter) {

  SensorData receivedData;

  while (1) {

    xEventGroupWaitBits(
      sensorEvents,
      SENSOR_DATA_READY,
      pdTRUE,
      pdFALSE,
      portMAX_DELAY
    );

    if (xQueueReceive(
          displayQueue,
          &receivedData,
          pdMS_TO_TICKS(100)) == pdTRUE) {

      if (xSemaphoreTake(
            lcdMutex,
            pdMS_TO_TICKS(100)) == pdTRUE) {

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("Sensor: ");
        lcd.print(receivedData.digitalValue);

        lcd.setCursor(0, 1);

        if (receivedData.digitalValue == 0) {
          lcd.print("STATUS: ACTIVE");
        } else {
          lcd.print("STATUS: IDLE");
        }

        xSemaphoreGive(lcdMutex);
      }
    }
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_DO, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  // Queues
  controlQueue = xQueueCreate(
    5,
    sizeof(SensorData)
  );

  displayQueue = xQueueCreate(
    5,
    sizeof(SensorData)
  );

  // Mutexes
  lcdMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();

  // Event group
  sensorEvents = xEventGroupCreate();

  if (controlQueue == NULL ||
      displayQueue == NULL ||
      lcdMutex == NULL ||
      serialMutex == NULL ||
      sensorEvents == NULL) {

    Serial.println("RTOS object creation failed!");

    while (1);
  }

  Serial.println("================================");
  Serial.println("ESP32 FreeRTOS System Started");
  Serial.println("================================");

  // Sensor task
  xTaskCreate(
    sensorTask,
    "Sensor Task",
    2048,
    NULL,
    2,
    NULL
  );

  // Control task
  xTaskCreate(
    controlTask,
    "Control Task",
    2048,
    NULL,
    2,
    NULL
  );

  // LCD task
  xTaskCreate(
    lcdTask,
    "LCD Task",
    2048,
    NULL,
    1,
    NULL
  );
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  // FreeRTOS handles the application.

}