#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <nvs.h>
#include <nvs_flash.h>

const char * path = "/intiator_logs.txt";
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

/////////////////////////////////////   RSSI  //////////////////////////////////////

int rssi_display;

// Estructuras para calcular los paquetes, el RSSI, etc
typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; // receiver address 
  uint8_t addr2[6]; // sender address 
  uint8_t addr3[6]; // filtering address 
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; // optional 
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; // network data ended with 4 bytes csum (CRC32) 
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

// MAC Address of responder - edit as required
// uint8_t bcMACs[10][6] = {
//   {0x0C,0xB8,0x15,0xD8,0x2D,0x34},// E3
//   {0x0C,0xB8,0x15,0xD8,0x34,0x48},
//   {0x0C,0xB8,0x15,0xD7,0x99,0x58},
//   {0x0C,0xB8,0x15,0xD7,0x8F,0x2C},// E0
//   {0x0C,0xB8,0x15,0xD6,0x0E,0xA8},
//   {0x0C,0xB8,0x15,0xD7,0x90,0xDC},
//   {0x0C,0xB8,0x15,0xD8,0x22,0x34},
//   {0x0C,0xB8,0x15,0xD8,0x27,0x10},
//   {0x0C,0xB8,0x15,0xD8,0x21,0x34},
//   {0xC8,0xC9,0xA3,0xFB,0xE6,0xC0} // ttgo1
// };

const int num_of_slaves = 1;

uint8_t bcMACs[num_of_slaves][6] = {
  // {0x0C,0xB8,0x15,0xD7,0x82,0x28}, // E1
  // {0x0C,0xB8,0x15,0xD7,0x99,0x58}, // E2
  {0x0C,0xB8,0x15,0xD6,0x0E,0xA8}, // E4
  // {0x0C,0xB8,0x15,0xD8,0x22,0x34}, // E6
  // {0x0C,0xB8,0x15,0xD8,0x27,0x10} // E7
  // {0x78,0x21,0x84,0x7C,0x98,0xD8} // 1
  // {0x84,0xCC,0xA8,0x57,0xCE,0x2C}, // 2
  // {0x0C,0xB8,0x15,0xD7,0x90,0xDC}, // E5
  // {0x4C,0x11,0xAE,0x9F,0x71,0x04}, // E9
};

// Define a data structure with fixed size
typedef struct struct_message {
  unsigned long time;
  unsigned int packetNumber;
  // const char msg[100] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  // uint8_t arr[240];
} struct_message;
int dataSize = 8;

// Create a structured object
struct_message myData;

esp_now_peer_info_t slaveInfo;

// PMK and LMK keys
static const char* PMK_KEY_STR = "PLEASE_CHANGE_ME";
static const char* LMK_KEY_STR = "DONT_BE_LAZY_OK?";

esp_err_t result;
unsigned long packetSentTime;
int nodeIndex = 0;

// Callback function called when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  unsigned long packetGetTime = millis();
  Serial.print('E');Serial.print(nodeIndex);Serial.print('\t');
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(status == ESP_NOW_SEND_SUCCESS ? "\tS\t" : "\tF\t");
  Serial.print(packetSentTime);Serial.print('\t');
  Serial.print(packetGetTime);Serial.print('\t');
  Serial.println(rssi_display);
}

void clearNVS() {
  int err;
  err = nvs_flash_init();
  Serial.println("nvs_flash_init:"+ err);
  err = nvs_flash_erase();
  Serial.println("nvs_flash_erase:"+ err);
}

void setup() {

  // Set up Serial Monitor
  Serial.begin(115200);

  // clearNVS();

  // for (int i = 0; i < 100; i++) {
  //   myData.msg[i] = "a";
  //   Serial.printf("%c", myData.msg[i]);
  // }
  // for (int i = 0; i < 242; i++) {
  //   myData.arr[i] = 1;
  // }
  // Serial.println(sizeof(myData.arr));

  //Init sd card
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }

  myData.packetNumber = 1;

  // WiFi.useStaticBuffers(true);
  // Set ESP32 as a Wi-Fi AP
  WiFi.mode(WIFI_STA);
  esp_wifi_start();
  
  int a = esp_wifi_set_protocol( WIFI_IF_AP, WIFI_PROTOCOL_LR);
  esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_LORA_250K);
  // int a = esp_wifi_set_protocol( WIFI_IF_AP, (WIFI_PROTOCOL_11B| WIFI_PROTOCOL_11G| WIFI_PROTOCOL_11N));
  // esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_1M_L);

  int b = esp_wifi_set_max_tx_power(80);
  Serial.println(b);

  // Serial.print("WIFI LR ");
  // Serial.println(a);

  // sprintf(str, "WIFI LR %d\n", a); 
  // Serial.printf(str);
  // appendFile(SD, path, str);

  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    // Serial.println("Error initializing ESP-NOW");
    sprintf(str, "Error initializing ESP-NOW\n");
    Serial.printf(str);
    appendFile(SD, path, str);
    return;
  }

  // Setting the PMK key
  esp_now_set_pmk((uint8_t *)PMK_KEY_STR);

  // Register the slaves
  for(int i=0; i<num_of_slaves; i++)
  {
    memcpy(slaveInfo.peer_addr, bcMACs[i], 6);
    if (esp_now_add_peer(&slaveInfo) != ESP_OK){
      sprintf(str, "Failed to add peer: %d\n", i);
      Serial.printf(str);
      appendFile(SD, path, str);
      // Serial.print("Failed to add peer: ");
      // Serial.println(i);
      return;
    }
  }
  
  slaveInfo.channel = 0;
  // Setting the master device LMK key
  for (uint8_t i = 0; i < 16; i++) {
    slaveInfo.lmk[i] = LMK_KEY_STR[i];
  }
  slaveInfo.encrypt = false;
  // Register the send callback
  esp_now_register_send_cb(OnDataSent);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

  // 5 min for setup
  // delay(1000*60*0.5);
    delay(1000*5);

  // Serial.println("START");
  sprintf(str, "START\n");
  Serial.printf(str);
  appendFile(SD, path, str);
}

void loop() {
  while (myData.packetNumber <= 1000) {
    for(int i=0; i<num_of_slaves; i++)
    {
      nodeIndex = i;
      packetSentTime = millis();
      myData.time = packetSentTime;
      esp_now_send(bcMACs[i], (uint8_t *) &myData, sizeof(myData));
      //Serial.println(((result == ESP_OK)?"Sending confirmed":"Sending error"));
      sprintf(str, "sending packet %d to %d node\n", myData.packetNumber, i);
      Serial.printf(str);
      appendFile(SD, path, str);
      delay(500);
    }
    myData.packetNumber++;
  }
}
