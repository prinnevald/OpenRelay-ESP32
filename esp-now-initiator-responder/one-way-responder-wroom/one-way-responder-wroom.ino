// BY NU IOT LAB //
// REFERENCES:
// rssi: https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino

// Include Libraries
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <nvs.h>
#include <nvs_flash.h>

///////////////////////// MAC STUFF, ENCRYPTION AND DATA /////////////////////////////


// Set the MASTER MAC Address
uint8_t masterAddress[] = {0xC8,0xC9,0xA3,0xFB,0xF7,0xA4}; // TTGO2
int packetReceived = 0;

// PMK and LMK keys
static const char* PMK_KEY_STR = "PLEASE_CHANGE_ME";
static const char* LMK_KEY_STR = "DONT_BE_LAZY_OK?";

esp_now_peer_info_t masterInfo;

// Define a data structure with fixed size
typedef struct struct_message {
  unsigned int packetNumber;
  unsigned long time;
  // char msg[100];
  // uint8_t arr[240];
} struct_message;
int dataSize = 108;

// Create a structured object
struct_message myData;

/////////////////////////////////////   RSSI  //////////////////////////////////////

int rssi_display;

// Estructuras para calcular los paquetes, el RSSI, etc
typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

//La callback que hace la magia
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  int rssi = ppkt->rx_ctrl.rssi;
  rssi_display = rssi;
}

//////////////////////////////////// END RSSI /////////////////////////////////

void clearNVS() {
  int err;
  err = nvs_flash_init();
  Serial.println("nvs_flash_init:"+ err);
  err = nvs_flash_erase();
  Serial.println("nvs_flash_erase:"+ err);
}

const char * path = "/responder_logs.txt";
char str[100];

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
    // Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void printArr(const uint8_t arr[]) {
  for (int i = 0; i < 240; i++) {
    Serial.printf("%d %i\n", i, arr[i]);
  }
  Serial.println();
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {

  packetReceived++;
  //display.clearDisplay();
  
  memcpy(&myData, incomingData, sizeof(myData));

  // Serial.println(sizeof(myData));
  Serial.print(packetReceived);
  Serial.print('\t');
  Serial.print(myData.time);
  Serial.print('\t');
  Serial.print(rssi_display);
  Serial.print('\t');
  Serial.print(myData.packetNumber);
  Serial.print('\t');
  Serial.println(len);
  // Serial.print('\t');
  // Serial.println(sizeof(myData.arr));
  // printArr(myData.arr);


  sprintf(str, "%d\t", packetReceived);
  appendFile(SD, path, str);

  sprintf(str, "%lu\t", myData.time);
  appendFile(SD, path, str);

  sprintf(str, "%i\t", rssi_display);
  appendFile(SD, path, str);

  sprintf(str, "%i\t", myData.packetNumber);
  appendFile(SD, path, str);

  sprintf(str, "%i\n", len);
  appendFile(SD, path, str);
  
  // delay(50);
}


void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);

  // clearNVS();

  //Init sd card
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  ESP_ERROR_CHECK( esp_wifi_start());
  
  int a = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_250K);
  // int a = esp_wifi_set_protocol(WIFI_IF_STA, (WIFI_PROTOCOL_11B| WIFI_PROTOCOL_11G| WIFI_PROTOCOL_11N));
  // esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_1M_L);

  int b = esp_wifi_set_max_tx_power(80);
  Serial.println(b);
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("There was an error initializing ESP-NOW");
    return;
  }

  // Setting the PMK key
  esp_now_set_pmk((uint8_t *)PMK_KEY_STR);

  // Register the master
  memcpy(masterInfo.peer_addr, masterAddress, 6);
  masterInfo.channel = 0;
  // Setting the master device LMK key
  for (uint8_t i = 0; i < 16; i++) {
    masterInfo.lmk[i] = LMK_KEY_STR[i];
  }
  masterInfo.encrypt = false;
  
  // Add master        
  if (esp_now_add_peer(&masterInfo) != ESP_OK){
    Serial.println("There was an error registering the master");
    return;
  }
  
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

}

void loop() {
  // Serial.println("WAAAAALLLL-EEEEEEEEEEEEEE");

  // delay(5);
}
