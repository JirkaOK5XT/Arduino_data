/*
   -- 4sw1text1var --
   Upraveno pro cyklické zobrazování pevných testovacích hodnot
*/


#define REMOTEXY_MODE__WIFI_CLOUD
#include <WiFi.h>
#include <RemoteXY.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <base64.h> // Součást jádra ESP32
// RemoteXY connection settings 
#define REMOTEXY_WIFI_SSID ""
#define REMOTEXY_WIFI_PASSWORD ""
#define REMOTEXY_CLOUD_SERVER "cloud.remotexy.com"
#define REMOTEXY_CLOUD_PORT 6376
#define REMOTEXY_CLOUD_TOKEN ""

const char* token = "";
const char* repo = "JirkaOK5XT/Arduino_data";
const char* path = "data.txt"; // Název souboru na GitHubu

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   
  { 255,4,0,11,0,98,0,19,0,0,0,83,119,105,116,99,104,101,114,0,
  196,1,106,200,1,1,5,0,2,6,5,91,31,0,1,0,31,31,79,78,
  0,79,70,70,0,2,5,41,93,33,0,12,0,31,31,79,78,0,79,70,
  70,0,2,5,79,94,33,0,6,0,31,31,79,78,0,79,70,70,0,67,
  6,161,92,26,69,13,0,11,2,4,117,92,32,0,94,0,31,31,79,78,
  0,79,70,70,0 };
  
struct {
    // input variables
  uint8_t switch_01; 
  uint8_t switch_02; 
  uint8_t switch_03; 
  uint8_t switch_04; 

    // output variables
  char value_01[11]; // string UTF8 end zero

    // other variable
  uint8_t connect_flag;  
} RemoteXY;   
#pragma pack(pop)

// TESTOVACÍ PEVNÁ DATA
int value_dcvolt = 302; // Testovací napětí
uint8_t O1 = (RemoteXY.switch_01), O2 = (RemoteXY.switch_02), O3 = (RemoteXY.switch_03), O4 = (RemoteXY.switch_04) ; // HMI switche
uint8_t I1 = 0, I2 = 1, I3 = 0, I4=0; // Testovací vstupy

unsigned long posledniPrepnuti = 0;
uint8_t stav = 0;
//*********************************
#include "driver/mcpwm_cap.h"

// Globální proměnné
volatile uint32_t pos_edge_time = 0;
volatile uint32_t period_ticks = 0;
volatile uint32_t high_ticks = 0;

// Minimalistický callback
static bool IRAM_ATTR on_capture_handler(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data) {
    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
        period_ticks = edata->cap_value - pos_edge_time;
        pos_edge_time = edata->cap_value;
    } else {
        high_ticks = edata->cap_value - pos_edge_time;
    }
    return false; 
}
// Definice pinů v řadě za sebou
const uint8_t tlacitka[] = {13, 12, 14, 27}; 
const uint8_t pocetTlacitek = 4;
// 4 digitální výstupy v řadě od GPIO 33
const uint8_t vystupy[] = {33, 32, 35, 34};
const uint8_t pocetVystupu = 4;

int rssi = WiFi.RSSI();
int temp1 = 0;
int temp2 = 0;
int temp3 = 0;

// Definice pinu a počtu senzorů
#define ONE_WIRE_BUS 4
#define EXPECTED_SENSORS 3

// Inicializace instance OneWire pro komunikaci s libovolným zařízením OneWire
OneWire oneWire(ONE_WIRE_BUS);

// Předání reference na OneWire do knihovny DallasTemperature
DallasTemperature sensors(&oneWire);

void setup() 
{
  // Start knihovny
  sensors.begin();

  // Kontrola počtu připojených senzorů
  int foundDevices = sensors.getDeviceCount();

  // Nastavení rozlišení na 12 bitů (nejpřesnější měření, nutné pro vysoké teploty)
  sensors.setResolution(12);

  RemoteXY_Init(); 
  //***************************
   //Serial.begin(115200);

    // 1. Timer
    mcpwm_cap_timer_handle_t cap_timer = NULL;
    mcpwm_capture_timer_config_t timer_conf;
    timer_conf.group_id = 0;
    timer_conf.clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT;
    mcpwm_new_capture_timer(&timer_conf, &cap_timer);

    // 2. Kanál - ZDE SI NASTAVTE SVŮJ PIN (nyní GPIO 18)
    mcpwm_cap_channel_handle_t cap_chan = NULL;
    mcpwm_capture_channel_config_t chan_conf;
    chan_conf.gpio_num = (gpio_num_t)18; 
    chan_conf.prescale = 1;
    chan_conf.flags.neg_edge = 1;
    chan_conf.flags.pos_edge = 1;
    chan_conf.flags.pull_up = 1;
    mcpwm_new_capture_channel(cap_timer, &chan_conf, &cap_chan);

    // 3. Callback
    mcpwm_capture_event_callbacks_t cbs;
    cbs.on_cap = on_capture_handler;
    mcpwm_capture_channel_register_event_callbacks(cap_chan, &cbs, NULL);

    // 4. Start
    mcpwm_capture_channel_enable(cap_chan);
    mcpwm_capture_timer_enable(cap_timer);
    mcpwm_capture_timer_start(cap_timer);

    //Serial.println("Start mereni...");


     for (int i = 0; i < pocetTlacitek; i++) {
    // Všechny mají interní Pull-Up (v klidu HIGH, stisk LOW)
    pinMode(tlacitka[i], INPUT_PULLUP);
  }
  for (int i = 0; i < pocetVystupu; i++) {
    pinMode(vystupy[i], OUTPUT);
    digitalWrite(vystupy[i], HIGH); // U Active LOW modulů toto relé VYPNUTÁ
}
}




void loop() 
{ 
  RemoteXY_Handler(); 
 uint8_t O1 = (RemoteXY.switch_01), O2 = (RemoteXY.switch_02), O3 = (RemoteXY.switch_03), O4 = (RemoteXY.switch_04) ; // HMI switche  
 int value_dcvolt = 252; // Testovací napětí
  // Přepínání textů každých 5 sekund (1200 ms)
  if (millis() - posledniPrepnuti > 1200) {
    posledniPrepnuti = millis();
    stav++;
    if (stav > 13) stav = 0; 
  }
int rssi = WiFi.RSSI();
int temp1 = ((WiFi.RSSI() * -1) -40) ;              //pouze test, pak bude cidlo
int temp2 = ((WiFi.RSSI() * -1) -42) ;              //pouze test, pak bude cidlo
int temp3 = ((WiFi.RSSI() * -1) -44) ;              //pouze test, pak bude cidlo
  // Zápis do textového pole RemoteXY
  switch (stav) {
    case 0:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "DC  %d V", value_dcvolt);
      break;
    case 1:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vystup 1:%d", O1);
      break;
    case 2:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vystup 2:%d", O2);
      break;
    case 3:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vystup 3:%d", O3);
      break;
    case 4:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vystup 4:%d", O4);
      break;
    case 5:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "DC  %d V", value_dcvolt);
      break;
    
    case 6:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vstup 1:%d", I1);
      break;
    case 7:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vstup 2:%d", I2);
      break;
    case 8:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vstup 3:%d", I3);
      break;
    case 9:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "Vstup 4:%d", I4);
      break;
    case 10:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "WiFi: %d", rssi);
      break;
    case 11:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "T1: %d°C", temp1 );
      break;
    case 12:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "T2: %d°C", temp2 );
      break;
    case 13:
      snprintf(RemoteXY.value_01, sizeof(RemoteXY.value_01), "T3: %d°C", temp3 );
      break;

  }
  //***********************
   if (period_ticks > 0) {
        // Výpočet střídy 0.0 - 1.0
        float duty = (float)high_ticks / (float)period_ticks;
        
        // Převod na 0 - 5000
        int value_5000 = (int)(duty * 5000.0f);
        if(value_5000 > 5000) value_5000 = 5000;
        if(value_5000 < 0) value_5000 = 0;

        //Serial.printf("Vysledek: %d\n", value_5000);
    }
    //delay(200);

    //*******************
      // Příkaz všem senzorům na sběrnici, aby zahájily měření
  sensors.requestTemperatures(); 

  for (int i = 0; i < EXPECTED_SENSORS; i++) {
    float tempC = sensors.getTempCByIndex(i);

   // Serial.print("Senzor ");
   // Serial.print(i + 1);
   // Serial.print(": ");

    // Validace dat (DS18B20 vrací specifické chyby)
    if (tempC == DEVICE_DISCONNECTED_C) {
     // Serial.println("CHYBA: Senzor odpojen!");
    } 
    else if (tempC == 85.00) {
      //Serial.println("CHYBA: Špatné napájení (85°C error)!");
    } 
    else {
      //Serial.print(tempC);
      //Serial.print(" °C");
      
      // Varování při kritické teplotě
      if (tempC > 120.0) {
       // Serial.print(" [!!! LIMITNÍ TEPLOTA !!!]");
      }
      //Serial.println();
    }
  }
// --- ODESÍLÁNÍ NA GITHUB ---
static unsigned long posledniGithubUpdate = 0;
if (millis() - posledniGithubUpdate > 300000) { // 300 000 ms = 5 minut
    posledniGithubUpdate = millis();

    // Vytvoření textového řetězce se všemi proměnnými
    String dataProGithub = "Log ze senzoru:\n";
    dataProGithub += "Teplota 1: " + String(temp1) + " C\n";
    dataProGithub += "Teplota 2: " + String(temp2) + " C\n";
    dataProGithub += "Teplota 3: " + String(temp3) + " C\n";
    dataProGithub += "Signal WiFi: " + String(rssi) + " dBm\n";
    dataProGithub += "DC Napeti: " + String(value_dcvolt) + " V\n";
    dataProGithub += "Stav vstupu: " + String(I1) + String(I2) + String(I3) + String(I4);
    dataProGithub += "Stav vystupu: " + String(O1) + String(O2) + String(O3) + String(O4);

    sendToGithub(dataProGithub);
  }
}
// podprogram volany v hlavi smycce - upload na github
void sendToGithub(String message) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  // URL adresa pro GitHub API
  String url = "https://api.github.com" + String("JirkaOK5XT/Arduino_data") + "/contents/" + String("data.txt");
  
  // Zakódování zprávy do Base64 (vyžadováno GitHubem)
  String encodedMessage = base64::encode(message);

  http.begin(url);
  http.addHeader("Authorization", "token " + String(token));
  http.addHeader("Content-Type", "application/json");

  // GitHub vyžaduje pro aktualizaci existujícího souboru "SHA" otisk. 
  // Pokud soubor neexistuje, vytvoří se. Pokud existuje, tento kód ho přepíše.
  // Poznámka: Pro složitější logování (přidávání řádků) by byl kód delší, 
  // toto slouží pro zobrazení aktuálního stavu v data.txt.
  
  String jsonPayload = "{\"message\":\"Automaticky update z ESP32\",\"content\":\"" + encodedMessage + "\"}";

  int httpResponseCode = http.PUT(jsonPayload); 

 /* if (httpResponseCode > 0) {
    Serial.print("GitHub Update OK. Kod: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Chyba pri odesilani: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }
  http.end();
  */
}

/*
Proč tento kód funguje pro +125 °C:
Rozlišení 12 bitů: Zajišťuje nejjemnější krok měření (0,0625 °C), což je u vysokých teplot žádoucí pro sledování trendu.
Kontrola chyby 85 °C: Tato hodnota se u DS18B20 objeví, pokud senzor dostane pokyn k měření, ale nemá dostatek proudu (např. při slabém pull-up rezistoru nebo dlouhém kabelu). U vysokých teplot je stabilní napájení klíčové.
Kritické varování: Kód vás upozorní, pokud se teplota dostane nad 120 °C, kdy už hrozí poškození běžné izolace kabelu.
Důležitá připomínka k zapojení:
Pull-up rezistor 4,7 kΩ dejte co nejblíže k ESP32 mezi piny 3.3V a GPIO 4.
Pokud jsou senzory na dlouhém kabelu (více než 5 metrů), snižte hodnotu rezistoru na cca 2,2 kΩ.
*/
