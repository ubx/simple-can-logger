#include <ESP32CAN.h>
#include <CAN_config.h>

#include <FS.h>
#include <SD.h>
#include <SPI.h>

#ifdef DEBUG
#define Sprint(a) (Serial.print(a))
#define Sprintln(a) (Serial.println(a))
#define Sprintf(a, b) (Serial.printf(a,b))
#else
#define Sprint(a)
#define Sprintln(a)
#define Sprintf(a, b)
#endif

#define LED 13
#define FLUSH_FREQ 1000

CAN_device_t CAN_cfg = {
        CAN_SPEED_500KBPS,
        GPIO_NUM_5,
        GPIO_NUM_4,
        xQueueGenericCreate((1000), (sizeof(CAN_frame_t)), (((uint8_t) 0U)))
};

char filename[40];
File file;

void setup() {
    Serial.begin(115200);
    if (!SD.begin()) {
        Sprintln("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Sprintln("No SD card attached");
        return;
    }

    Sprint("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Sprintln("MMC");
    } else if (cardType == CARD_SD) {
        Sprintln("SDSC");
    } else if (cardType == CARD_SDHC) {
        Sprintln("SDHC");
    } else {
        Sprintln("UNKNOWN");
    }

    Sprintf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    Sprintf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Sprintf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

    // create a unique file name
    int fn = 0;
    do {
        sprintf(filename, "/candump-%d.log", fn++);
    } while (SD.exists(filename));
    file = SD.open(filename, FILE_APPEND);

    //start CAN Module
    CAN_init();

    // set activity LED
    pinMode(LED, OUTPUT);
}

char buffer[100];
CAN_frame_t rx_frame;
int cnt = 0;

void loop() {
    //receive next CAN frame from queue
    if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {
        if (rx_frame.FIR.B.RTR == CAN_RTR) {
            // printf(" RTR from 0x%08x, DLC %d\r\n", rx_frame.MsgID, rx_frame.FIR.B.DLC);
        } else {
            digitalWrite(LED, HIGH);
            // printf(" from 0x%08x, DLC %d\n", rx_frame.MsgID, rx_frame.FIR.B.DLC);
            int j = sprintf(buffer, "(%010lu.%06lu) can0 %03X#", millis() / 1000, micros() % 1000000,
                            rx_frame.MsgID);
            for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
                j += sprintf(buffer + j, "%02X", rx_frame.data.u8[i]);
            }
            sprintf(buffer + j, "\n");
            if (file.print(buffer)) {
                Sprintf("Message appended: %d\n", cnt);
                cnt++;
                if (cnt > FLUSH_FREQ) {
                    file.flush();
                    Sprintf("Message flushed: %d\n", cnt);
                    cnt = 0;
                }
            } else {
                Sprintln("Append failed");
            }
            digitalWrite(LED, LOW);
        }
    }
}