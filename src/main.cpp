//               #----LIBRERIE----#
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <SPIFFS.h>
#include <ThingsBoard.h>
#include <Update.h>
#include <FS.h>
#include <SPI.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

//               #----COSTANTI----#

// LED
const int ledGreen = 16;
const int ledRed = 17;
const int ledBlue = 5;
const int freq = 5000;
const int ledWifi = 0;
const int ledTB = 1;
const int ledSubscribe = 2;
const int resolution = 8;

int sendTBlogSetup = 0;

// DISPLAY
#define SerialPrintLn(x) \
  logln(x);              \
  displayText(x)
#define SCREEN_WIDTH 128 // larghezza
#define SCREEN_HEIGHT 64 // altezza
#define OLED_RESET -1
#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16
boolean BOOT = true;

// FILE
#define FILE_CONFIG "/config.txt"
#define FILE_UPDATEURL "/ota_url.txt"
#define FILE_UPDATERESULT "/ota_result.txt"
#define FILE_UPDATEBIN "/update.bin"
#define CHUNK_SIZE 65536

// WIFI
const char ssid[] = "abinsula-28";
const char password[] = "uff1c10v14l3umb3rt028";

// THINGSBOARD
constexpr char THINGSBOARD_SERVER[] = "147.185.221.18";
const uint32_t THINGSBOARD_PORT = 62532U;
const char TOKEN[] = "ABOYl08Kk6OcE0gYzzbe";
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

//               #----DEFINIZIONI----#

// WIFI
WiFiClient espClient;

// MQTT CLIENT
Arduino_MQTT_Client mqttClient(espClient);

// THINGSBOARD
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// DISPLAY
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SENSORE
Adafruit_MPU6050 mpu;

// ATTRIBUTI SHARED
char sendDataInterval[] = "sendDataInterval";
long sendDataInterval_int = 1;

char displayIntervalRefresh[] = "displayIntervalRefresh";
long displayIntervalRefresh_int = 1;

char getDataInterval[] = "getDataInterval";
long getDataInterval_int = 1;

// UPDATE OTA (OVER THE AIR)
char urlUpdateBin[] = "";
HTTPClient http;
File updatebinfile;
boolean updating = false;
char FWVersion[] = "1.0";
int downloadTry = 0;

// VAR
unsigned long previousMillis = 0;
boolean attributesChanged = false;

//               #----OVERRIDE PRINT----#

// Manda a TB il log + stampa sul serial
void logln(const char *text)
{
  if (!updating)
  {
    tb.sendTelemetryData("LOGS", text);
  }
  Serial.println(text);
}
void log(const char *text)
{
  if (!updating)
  {
    tb.sendTelemetryData("LOGS", text);
  }
  Serial.print(text);
}
void logstr(String text)
{
  if (!updating)
  {
    tb.sendTelemetryData("LOGS", text);
  }
  Serial.println(text);
}
void logint(int text)
{
  if (!updating)
  {
    tb.sendTelemetryData("LOGS", text);
  }
  Serial.println(text);
}

//               #----FUNZIONI----#

void displayText(const char *testo)
{
  // Usato per visualizzare del testo sullo schermo
  // Pulisce, stampa e rilascia il buffer
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(testo);
  display.display();
  // Stampa in seriale per debug
  logln(testo);
}

void InitDisplay()
{
  // Inizializza il display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    for (;;)
      ; // Loop
  }
  display.setTextSize(1);      // Scala 1 a 1 per i pixel
  display.setTextColor(WHITE); // Imposta il colore a bianco
  display.setCursor(0, 0);     // Muove il cursore in alto a sinistra
  display.cp437(true);         // Includi tutti i caratteri speciali
  display.clearDisplay();      // Pulisci il display
  // Viene eseguito il "logo" solo una volta (boot)
  if (BOOT)
  {
    display.write("\n\n\n   Chessa & Pisano");
    display.write("\n\n\n\n  Avvio in corso...");
    BOOT = false;
  }
  // Rilascia il buffer
  display.display();
}

void InitWiFi()
{
  delay(10);
  logln("[INFO] WI-Fi] Tentativo di connessione a ");
  log(ssid);
  log("\n");
  // Tenta la connessione a tale ssid e pswd
  int i = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    i++;
    if (i > 30)
    {
      ESP.restart();
    }
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    logln("[INFO] WI-Fi] Wi-Fi connesso a ");
    log(ssid);
    log("\n");
    // Accende il led del wifi
    ledcWrite(ledWifi, 25);
  }
}

void InitMPU()
{
  // Inizializza il sensore
  if (!mpu.begin())
  {
    while (1)
    {
      delay(10);
    }
  }
  // Impostazioni di default (presi da libreria di base)
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
}

void DisplayMPU()
{
  // Mostra i valori del sensore a schermo
  sensors_event_t a, g, temp;
  // Ottieni i valori
  mpu.getEvent(&a, &g, &temp);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println();
  display.println();
  display.println("Giroscopio - rps");
  display.print(g.gyro.x, 1);
  display.print(", ");
  display.print(g.gyro.y, 1);
  display.print(", ");
  display.print(g.gyro.z, 1);
  display.println("");
  display.println("Accelerometro - m/s^2");
  display.print(a.acceleration.x, 1);
  display.print(", ");
  display.print(a.acceleration.y, 1);
  display.print(", ");
  display.print(a.acceleration.z, 1);
  display.println("");
  display.display();
  delay(50);
}

void sendMPUdata()
{
  // Tenta di mandare a ThingsBoard i dati del sensore
  sensors_event_t a, g, temp;
  logln("[INFO] TB - sendTelemetryData] Mando dati MPU a TB...");
  mpu.getEvent(&a, &g, &temp);

  // Per ogni tentativo..
  if (!tb.sendTelemetryData("DataGyroscopeX", g.gyro.x))
  {
    // logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataGyroscopeX");
    logln("[CRITICAL] TB - sendTelemetryData] Impossibile mandare i dati a TB - Timeout?");
    return;
  }
  if (!tb.sendTelemetryData("DataGyroscopeY", g.gyro.y))
  {
    logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataGyroscopeY");
  }
  if (!tb.sendTelemetryData("DataGyroscopeZ", g.gyro.z))
  {
    logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataGyroscopeZ");
  }
  if (!tb.sendTelemetryData("DataAccelerationX", a.acceleration.x))
  {
    logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataAccelerationX");
  }
  if (!tb.sendTelemetryData("DataAccelerationY", a.acceleration.y))
  {
    logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataAccelerationY");
  }
  if (!tb.sendTelemetryData("DataAccelerationZ", a.acceleration.z))
  {
    logln("[CRITICAL] TB - sendTelemetryData] Fallisco a mandare DataAccelerationZ");
  }
  logln("[INFO] TB - sendTelemetryData] Dati MPU mandati a TB");
}

void performUpdate()
{
  // Viene chiamato dopo il riavvio
  // Serve per flasharsi il nuovo firmware
  logln("[INFO] OTA] Preparo il nuovo firmware...");

  // Apre il nuovo firmware
  File updateFile = SPIFFS.open(FILE_UPDATEBIN, "r");
  if (!updateFile)
  {
    logln("[CRITICAL] OTA] Impossibile aprire il file!");
    return;
  }

  logln("[INFO] OTA] Calcolo dimensione firmware... ");

  // Serve a Update
  // Utile per capire se il firmware è stato scaricato correttamente
  // (debug: confronto questo valore con la dimensione effettiva)
  size_t updateSize = updateFile.size();

  log("[OTA] Dimensione firmware calcolato: ");
  logint(updateSize);

  // .begin inizia il flash
  if (Update.begin(updateSize))
  {
    // Tutto quello che c'è qui avviene dopo
    // la scrittura nel chip
    //
    // Quanto ha scritto Update nel chip
    size_t written = Update.writeStream(updateFile);
    if (written == updateSize)
    {
      logstr("[INFO] OTA] Scritti : " + String(written) + " byte correttamente");
    }
    else
    {
      logstr("[CRITICAL] OTA] Scritti soltato : " + String(written) + " byte /" + String(updateSize) + " byte. Riprova?");
    }
    // Se è finita la scrittura..
    if (Update.end())
    {
      // ..come è finita?
      if (Update.isFinished())
      {
        // Tutto il nuovo firmware è stato flashato
        logln("[INFO] OTA] Aggiornamento completato, riavvio...");
        SPIFFS.remove(FILE_UPDATEBIN);
        SPIFFS.remove(FILE_UPDATEURL);
        // updating serve per non mandare dati a ThingsBoard
        updating = true;
        // Riavvio
        ESP.restart();
      }
      else
      {
        // Non ha flashato correttamente
        logln("[CRITICAL] OTA] Aggiornamento fallito? Qualcosa è andato storto!");
        if (SPIFFS.exists(FILE_UPDATERESULT))
        {
          // Scrivi il risultato su un file
          File updateresult = SPIFFS.open(FILE_UPDATERESULT, "w");
          if (updateresult)
          {
            // Se scrive true significa che c'è è andato storto il flash
            // Al riavvio viene controllato questo valore e nel caso riscarica il nuovo firmware
            updateresult.print("true");
            updateresult.close();
          }
        }
      }
    }
    else
    {
      logstr("[CRITICAL] OTA] Errore #: " + String(Update.getError()));
      SPIFFS.remove(FILE_UPDATEBIN);
    }
  }
  else
  {
    logln("[CRITICAL] OTA] Non c'è abbastanza spazio per eseguire l'OTA");
  }
}

boolean checkJson(byte *payload, unsigned int length)
{
  // Chiamato quando si ricevono nuovi valori degli attributi / OTA da ThingsBoard
  // Spezzetta il json in multiple chiavi
  char jsonString[length + 1];
  memcpy(jsonString, payload, length);
  jsonString[length] = '\0';

  Serial.println("*********************");
  Serial.write(payload, length);
  Serial.println("");
  Serial.println("*********************");

  DynamicJsonDocument jsonDocument(2048);
  DeserializationError error = deserializeJson(jsonDocument, jsonString);

  if (error)
  {
    log("[WARNING] TB - callback/checkJson] Parsing fallito! Errore: ");
    logln(error.c_str());
    return true;
  }

  // Se contiene restartesp restarto
  if (jsonDocument.containsKey("restartesp"))
  {
    log("[ESP32] Riavvio richiesto da ThingsBoard...");
    delay(500);
    ESP.restart();
  }

  // Se ThingsBoard ci dice che ci sono delle varibili
  // vuote, semplicemente ce ne sbattiamo
  if (jsonDocument.containsKey("deleted"))
  {
    return true;
  }

  // Qui si controlla se c'è una chiave "fw_version"
  // e se presente aggiorna la versione del firmware
  if (jsonDocument.containsKey("fw_version"))
  {
    strcpy(FWVersion, jsonDocument["fw_version"]);
    DynamicJsonDocument completeConfig(1024);
    // Ricostruisce la configurazione
    // (perché dentro la configurazione c'è la versione del firmware)
    completeConfig["fw_version"] = FWVersion;
    completeConfig["sendDataInterval"] = sendDataInterval_int;
    completeConfig["displayIntervalRefresh"] = displayIntervalRefresh_int;
    completeConfig["getDataInterval"] = getDataInterval_int;
    File configFileOTA = SPIFFS.open(FILE_CONFIG, "w");
    if (configFileOTA)
    {
      serializeJsonPretty(completeConfig, configFileOTA);
      configFileOTA.close();
      Serial.println("[INFO] OTA | FW Version] Scrittura completata");
    }
    else
    {
      Serial.println("[CRITICAL] OTA | FW Version] Scrittura fallita!");
    }
  }

  // Qui si controlla se c'è la chiave "fw_url"
  // usata per ricavare l'url del nuovo firmware dalla richiesta OTA
  // (quando si riceve il link tramite widget di TB)
  if (jsonDocument.containsKey("fw_url"))
  {
    strcpy(urlUpdateBin, jsonDocument["fw_url"]);

    // Salva il link dentro un file
    // verrà usato solo se fallisce l'Update vero e proprio
    File ota1 = SPIFFS.open(FILE_UPDATEURL, "w");
    if (ota1)
    {
      ota1.print(urlUpdateBin);
      ota1.close();
      Serial.println("[INFO] OTA | FW URL] Scrittura completata");
    }
    else
    {
      Serial.println("[CRITICAL] OTA | FW URL] Scrittura fallita!");
    }

    // Crea (o apre) il file che usiamo per
    // capire se il chip è stato flashato correttamente
    // Se l'Update fallisce viene messo a true
    File ota2 = SPIFFS.open(FILE_UPDATERESULT, "w");
    if (ota2)
    {
      ota2.print("false");
      ota2.close();
      Serial.println("[INFO] OTA | FW RESULT] Scrittura completata");
    }
    else
    {
      Serial.println("[CRITICAL] OTA | FW RESULT] Scrittura fallita!");
    }

    return false;
  }

  // Un secondo modo da ThingsBoard per ricevere il link
  // (assegnando all'entità un firmware)
  if (jsonDocument.containsKey("targetFwUrl"))
  {
    strcpy(urlUpdateBin, jsonDocument["targetFwUrl"]);

    // Salva il link dentro un file
    // verrà usato solo se fallisce l'Update vero e proprio
    File ota1 = SPIFFS.open(FILE_UPDATEURL, "w");
    if (ota1)
    {
      ota1.print(urlUpdateBin);
      ota1.close();
      Serial.println("[INFO] OTA | FW URL] Scrittura completata");
    }
    else
    {
      Serial.println("[CRITICAL] OTA | FW URL] Scrittura fallita!");
    }

    return false;
  }

  // Controllo e imposto i nuovi valori degli attributi
  // ricevuti da ThingsBoard
  if (jsonDocument.containsKey("sendDataInterval"))
  {
    //                                   Converto la stringa a intero
    sendDataInterval_int = jsonDocument["sendDataInterval"].as<int>();
  }

  if (jsonDocument.containsKey("displayIntervalRefresh"))
  {
    displayIntervalRefresh_int = jsonDocument["displayIntervalRefresh"].as<int>();
  }

  if (jsonDocument.containsKey("getDataInterval"))
  {
    getDataInterval_int = jsonDocument["getDataInterval"].as<int>();
  }

  // Ricostruisco la configurazione con i nuovi valori
  DynamicJsonDocument completeConfig(1024);
  completeConfig["fw_version"] = FWVersion;
  completeConfig["sendDataInterval"] = sendDataInterval_int;
  completeConfig["displayIntervalRefresh"] = displayIntervalRefresh_int;
  completeConfig["getDataInterval"] = getDataInterval_int;
  File configFile = SPIFFS.open(FILE_CONFIG, "w");
  if (configFile)
  {
    serializeJsonPretty(completeConfig, configFile);
    configFile.close();
    logln("[INFO] TB - callback/checkJson] Scrittura completata");
    logln("[INFO] checkJson] Configurazione ricostruita");
  }
  else
  {
    logln("[CRITICAL] TB - callback/checkJson] Scrittura fallita!");
    logln("[INFO] checkJson] Configurazione NON ricostruita, write failed");
  }

  // Se gli attributi sono cambiati
  // skippo un ciclo di invio dati a ThingsBoard nel loop
  attributesChanged = true;
  logln("[INFO] checkJson] Attributi cambiati");
  return true;
}

//               #----DOWNLOAD NUOVO FIRMWARE OTA----#

boolean download()
{
  String url;
  updating = true;

  if (SPIFFS.exists(FILE_UPDATEURL))
  {
    File updateurl = SPIFFS.open(FILE_UPDATEURL, "r");
    if (updateurl)
    {
      Serial.println("[INFO] OTA] Aperto file FILE_UPDATEURL");
      url = updateurl.readString();
      url.toCharArray(urlUpdateBin, sizeof(urlUpdateBin));
      updateurl.close();
    }
  }
  else
  {
    Serial.println("[WARNING] OTA] FILE_UPDATEURL non esiste... uso la variabile globale");
    if (urlUpdateBin == "")
    {
      url = urlUpdateBin;
    }
    else
    {
      Serial.println("[CRITICAL] OTA] Variabile globale vuota. Impossibile procedere!");
      return false;
    }
  }

  // Formattazione dell'url
  log("[OTA | FW_URL] Richiedo il file ");
  logln(FILE_UPDATEBIN);

  // ********************* INIT HTTP *********************
  HTTPClient http;
  if (!http.begin(url))
  {
    Serial.println("[INFO] OTA] http.begin(url) fallito");
    return false;
  }

  // ********************* SEND GET REQUEST *********************
  size_t try_counter = 0;
  const size_t TRY_LIMIT = 20;
  int httpCode = -1;
  do
  {
    httpCode = http.GET();
    Serial.print(".");
    vTaskDelay(pdMS_TO_TICKS(250));
    if (try_counter++ == TRY_LIMIT)
    {
      Serial.println("[INFO] OTA] Connessione fallita con errore 408: Timeout");
      return false;
    }
  } while (httpCode != HTTP_CODE_OK);
  Serial.println("[INFO] OTA] GET Success");

  // ********************* RECEIVE FILE STREAM *********************
  WiFiClient *stream = http.getStreamPtr();
  try_counter = 0;
  do
  {
    stream = http.getStreamPtr();
    vTaskDelay(pdMS_TO_TICKS(250));
    Serial.print(".");
    if (try_counter++ == TRY_LIMIT)
    {
      Serial.println("[INFO] OTA] Connessione fallita con errore 408: Timeout");
      return false;
    }
  } while (!stream->available());
  Serial.println("[INFO] OTA] Ricevuto stream di dati");

  // ********************* CREATE NEW FILE *********************
  File file = SPIFFS.open(FILE_UPDATEBIN, FILE_APPEND);
  if (!file)
  {
    Serial.println("[INFO] OTA] Errore durante l'apertura del nuovo firmware");
    return false;
  }
  Serial.println("[INFO] OTA] Aperto il file FILE_UPDATEBIN...");

  // ********************* DOWNLOAD PROCESS *********************
  uint8_t *buffer_ = (uint8_t *)malloc(CHUNK_SIZE);
  uint8_t *cur_buffer = buffer_;
  const size_t TOTAL_SIZE = http.getSize();
  Serial.print("[INFO] OTA] Buffer totale da scaricare pari a ");
  Serial.print(TOTAL_SIZE);
  Serial.println(" bytes");
  size_t downloadRemaining = TOTAL_SIZE;
  size_t downloadRemainingBefore = downloadRemaining;
  Serial.println("[DOWNLOAD START] OTA] OK");

  int i = 0;
  auto start_ = millis();
  if (!http.connected())
  {
    Serial.println("[INFO] OTA] Condizione \"http.connected()\" risulta false?");
    return false;
  }
  while (downloadRemaining > 0 && http.connected())
  {
    if (downloadRemaining != downloadRemainingBefore)
    {
      i++;
      Serial.print("[INFO] OTA] In download Chunk ");
      Serial.print(i);
      Serial.print(" - ");
      Serial.print(downloadRemaining);
      Serial.print(" bytes rimanenti\n");
      downloadRemainingBefore = downloadRemaining;
    }
    auto data_size = stream->available();
    if (data_size > 0)
    {
      auto available_buffer_size = CHUNK_SIZE - (cur_buffer - buffer_);
      auto read_count = stream->read(cur_buffer, ((data_size > available_buffer_size) ? available_buffer_size : data_size));
      cur_buffer += read_count;
      downloadRemaining -= read_count;
      if (cur_buffer - buffer_ == CHUNK_SIZE)
      {
        //COMMIT
        for (int j = 0; j < read_count; j++)
        {
          file.print((char)buffer[j]);
        }
        cur_buffer = buffer_;
      }
    }
    vTaskDelay(1);
  }
  auto end_ = millis();

  // Scrivi eventuali dati rimanenti nel buffer
  // if (cur_buffer > buffer_)
  // {
  //   file.write(buffer_, cur_buffer - buffer_);
  // }

  Serial.println("[DOWNLOAD END] OTA] OK");

  size_t time_ = (end_ - start_) / 1000;
  String speed_ = String(TOTAL_SIZE / time_);
  Serial.println("[INFO] OTA] Velocità: " + speed_ + " bytes/sec");

  file.close();
  free(buffer_);

  File bin = SPIFFS.open(FILE_UPDATEBIN, "r");
  if (!bin)
  {
    Serial.println("[INFO] OTA] File non esistente?");
  }
  else
  {
    Serial.print("[INFO] OTA] Informazioni FIRMWARE.BIN scaricato: ");
    Serial.print(bin.size());
    Serial.print(" bytes\n");
    if (bin.size() == 0)
    {
      log("[CRITICAL] OTA] FIRMWARE.BIN non è stato scaricato correttamente.");
      return false;
    }
    bin.close();
  }

  return true;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  // Viene chiamato in automatico quando si aggiornano
  // i valori degli attributi o si manda un OTA
  //
  // Se questo ritorna come false è un OTA
  if (!checkJson(payload, length))
  {
  ritentadownload:
    if (download())
    {
      logln("[INFO] OTA] Download completato; Riavvio...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      if (downloadTry < 3)
      {
        logln("[CRITICAL] OTA] Download fallito; Riprovo...");
        delay(1000);
        downloadTry++;
        goto ritentadownload;
      }
      else
      {
        logln("[CRITICAL] OTA] Download fallito; Riavvio...");
        delay(1000);
        ESP.restart();
      }
    }
  }
}

void setupLed()
{
  // Inizializza i LED
  ledcSetup(ledWifi, freq, resolution);
  ledcSetup(ledTB, freq, resolution);
  ledcSetup(ledSubscribe, freq, resolution);
  ledcAttachPin(ledGreen, ledWifi);
  ledcAttachPin(ledRed, ledTB);
  ledcAttachPin(ledBlue, ledSubscribe);
  ledcWrite(ledWifi, 0);
  ledcWrite(ledTB, 0);
  ledcWrite(ledSubscribe, 0);
  delay(10);
}

void setupMainDirectory()
{
  // Collezione di cose per gestire la directory
  // e i suoi file
  //
  // Inizializza SPIFFS
  if (!SPIFFS.begin())
  {
    logln("[CRITICAL] SPIFFS - setupMainDirectory] SPIFFS non è stato inizializzato correttamente");
    logln("[WARNING] SPIFFS - setupMainDirectory] Riavvio dell'ESP...");
    ESP.restart();
  }
  else
  {
    logln("[INFO] SPIFFS - setupMainDirectory] SPIFFS inizializzato correttamente");
  }

  // Informazioni sullo stato SPIFFS
  // logln("[INFO] SPIFFS - setupMainDirectory] Informazioni sul file system:");
  // size_t totalBytes = LittleFS.totalBytes();
  // size_t usedBytes = LittleFS.usedBytes();
  // size_t remainingBytes = totalBytes - usedBytes;

  // Serial.println(String(totalBytes) + " bytes totali");
  // Serial.println(String(usedBytes) + " bytes usati");
  // Serial.println(String(remainingBytes) + " bytes rimanenti");

  // Se c'è un file di risultato flash
  // lo apri..
  if (SPIFFS.exists(FILE_UPDATERESULT))
  {
    File updateresult = SPIFFS.open(FILE_UPDATERESULT, "r");
    if (updateresult)
    {
      String content = updateresult.readString();
      updateresult.close();
      // ..e se c'è scritto true
      // riscarica il nuovo firmware
      if (content == "true")
      {
        logln("[INFO] OTA] C'è stato un errore nel download, ritento download...");
        download();
      }
    }
  }

  // Quando si scarica e si riavvia, si controlla
  // se c'è un nuovo firmware. Se c'è parte il flash
  if (SPIFFS.exists(FILE_UPDATEBIN))
  {
    logln("[INFO] OTA] Il download è stato completato; Aggiornamento del chip...");
    performUpdate();
  }

  // Apertura del file di configurazione
  File configFile = SPIFFS.open(FILE_CONFIG, "r");
  if (!configFile)
  {
    logln("[CRITICAL] SPIFFS - setupMainDirectory] Impossibile aprire il file di configurazione!");
    return;
  }
  size_t size = configFile.size();
  if (size > 1024)
  {
    logln("[CRITICAL] SPIFFS - setupMainDirectory] Dimensione del file troppo grande per essere caricato in memoria!");
    return;
  }

  // Lettura configurazione e deserializzo il json
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  DynamicJsonDocument jsonDocument(1024);
  DeserializationError error = deserializeJson(jsonDocument, buf.get());

  if (error)
  {
    log("[WARNING] SPIFFS - setupMainDirectory] Parsing fallito! Errore: ");
    logln(error.c_str());
    return;
  }

  // Usato dopo per capire quante chiavi ha letto
  int conteggioKey = 0;
  // Per ogni valore (se presente) aggiorni le variabili
  // in runtime
  if (jsonDocument.containsKey("fw_version"))
  {
    strcpy(FWVersion, jsonDocument["fw_version"]);
    conteggioKey++;
  }
  if (jsonDocument.containsKey("sendDataInterval"))
  {
    sendDataInterval_int = jsonDocument["sendDataInterval"].as<int>();
    conteggioKey++;
  }
  if (jsonDocument.containsKey("displayIntervalRefresh"))
  {
    displayIntervalRefresh_int = jsonDocument["displayIntervalRefresh"].as<int>();
    conteggioKey++;
  }
  if (jsonDocument.containsKey("getDataInterval"))
  {
    getDataInterval_int = jsonDocument["getDataInterval"].as<int>();
    conteggioKey++;
  }

  // Risultato
  if (conteggioKey == 0)
  {
    Serial.printf("[CRITICAL] SPIFFS - setupMainDirectory] Lettura fallita: 0 chiavi lette\n");
  }
  else if (conteggioKey == 1)
  {
    Serial.printf("[SPIFFS - setupMainDirectory] Lettura completata: 1 chiave letta\n");
  }
  else if (conteggioKey > 1)
  {
    Serial.printf("[SPIFFS - setupMainDirectory] Lettura completata: %d chiavi lette\n", conteggioKey);
  }

  logln("[INFO] SPIFFS - setupMainDirectory] JSON letto dal file:");
  serializeJsonPretty(jsonDocument, Serial);
  logln("");
  logln("[INFO] SPIFFS - setupMainDirectory] --------------------");

  File root = SPIFFS.open("/", "r");

  File file = root.openNextFile();

  while (file)
  {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print(";;; ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
}

//               #----SETUP | LOOP----#
void setup()
{
  Serial.begin(115200);
  logln("[INFO] SETUP] INIZIO SETUP!!");
  setupLed();
  logln("[INFO] SETUP] setupLed() chiamato");
  delay(300);
  InitDisplay();
  logln("[INFO] SETUP] InitDisplay() chiamato");
  delay(300);
  InitMPU();
  logln("[INFO] SETUP] InitMPU() chiamato");
  delay(300);
  InitWiFi();
  logln("[INFO] SETUP] InitWiFi() chiamato");
  delay(300);
  setupMainDirectory();
  logln("[INFO] SETUP] setupMainDirectory() chiamato");
  delay(300);
  //
  log("[ESP32] Firmware Versione ");
  logln(FWVersion);
  //
  mqttClient.set_callback(callback);
  logln("[INFO] SETUP] Callback mqttClient impostato");
  logln("[INFO] SETUP] FINE SETUP!");
  Serial.println("FINE SETUP DEL C");
}

void loop()
{
  // Se non si sta aggiornando l'ESP..
  if (!updating)
  {
    // ..se il wifi è connesso..
    if (!WiFi.status() == WL_CONNECTED)
    {
      InitWiFi();
    }
    // ..se sono connesso a ThingsBoard..
    if (!tb.connected() || !mqttClient.connected())
    {
      tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
    }
    else
    {
      // Accendi il led di ThingsBoard
      ledcWrite(ledTB, 25);
      if (mqttClient.subscribe("v1/devices/me/attributes"))
      {
        // e appena ti iscrivi al topic accendi anche un'altro led
        ledcWrite(ledSubscribe, 25);
        if (sendTBlogSetup == 0)
        {
          sendTBlogSetup = 1;
          logln("[ESP32] Setup completato con successo");
          logln("[ESP32] Procedo al normale funzionamento...");
        }
      }
      else
      {
        sendTBlogSetup = 0;
      }
    }
    // ..se gli attributi NON sono cambiati allora mandi i dati del
    // sensore a ThingsBoard
    if (!attributesChanged)
    {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= sendDataInterval_int * 1000)
      {
        sendMPUdata();
      }
      currentMillis = millis();

      delay(200);
      if (currentMillis - previousMillis >= displayIntervalRefresh_int * 1000)
      {
        previousMillis = currentMillis;
        if (!updating)
        {
          DisplayMPU();
        }
      }
    }
    else
    {
      attributesChanged = false;
    }

    mqttClient.loop();
    tb.loop();
  }
}