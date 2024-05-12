#include <Arduino.h>
#include <nrf.h>
#include <bluefruit.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <Adafruit_SPIFlash.h>  // Need to be deleted /Documents/Arduino/libraries/SdFat

// For battery control
#define PIN_VBAT     32     // D32 = P0.31 (AIN7_BAT)
#define VBAT_ENABLE  14     // D14 = P0.14 (READ_BAT)
#define HICHG   22  // D22 = P0.13 (BQ25100 ISET)

// BLE製造元で追加するデータです。先頭2バイトはベンダ名です。0xFFFFはテスト利用。
const uint8_t manufactData[2] = {0xFF,0xFF};

// BME280用変数
Adafruit_BME280 bme; // I2C

// ループ時間の待ち時間を設定します。
unsigned long delayTime;

// センサから読み取った値を格納する変数です。
float temp,humidity,pressure,vbat;

// スリープ時電力消費を押させる為に必要となる為
Adafruit_FlashTransport_QSPI flashTransport;

// CO2センサありフラグ
//#define CO2

#ifdef CO2

// CO2センサ用コマンドです。
byte ReadCO2[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
byte SelfCalOn[9]  = {0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};
byte SelfCalOff[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
byte retval[9];
// CO2読み取り値の保管用
uint16_t uartco2;

// BLEでアドバタイズするデータのフォーマットです。
// 0xFC,0xBE
// 0x01
// 4byte identifier(BLEのMACアドレスの下4バイトを後で設定します。)
// 0x10 (温度) + 2 byte data
// 0x11 (湿度) + 2 byte data
// 0x14 (気圧) + 2 byte data
// 0x17 (CO2) + 2 byte data
byte serviceData[21] = {
    // Musenconect Open sensor
  0xBE,0xFC,
  // fix value
  0x01,
  // 4 byte identifier
  0x00,0x00,0x00,0x00,
  // 温度
  0x10,0x00,0x00,
  // 湿度
  0x11,0x00,0x00,
  // 気圧
  0x14,0x00,0x00,
    // Battery voltage
  0x40,0x00,
  // CO2
  0x17,0x00,0x00
};

#else

// BLEでアドバタイズするデータのフォーマットです。
// 0xFC,0xBE
// 0x01
// 4byte identifier(BLEのMACアドレスの下4バイトを後で設定します。)
// 0x10 (温度) + 2 byte data
// 0x11 (湿度) + 2 byte data
// 0x14 (気圧) + 2 byte data

byte serviceData[18] = {
    // Musenconect Open sensor
  0xBE,0xFC,
  // fix value
  0x01,
  // 4 byte identifier
  0x00,0x00,0x00,0x00,
  // 温度
  0x10,0x00,0x00,
  // 湿度
  0x11,0x00,0x00,
  // 気圧
  0x14,0x00,0x00,
  // Battery voltage
  0x40,0x00

};

#endif



void startAdv(void)
{
  // Set parameters for advertising packet
  Bluefruit.Advertising.addManufacturerData(&manufactData, sizeof(manufactData));

  // Only needed in forced mode! In normal mode, you can remove the next line.
  bme.takeForcedMeasurement(); // has no effect in normal mode
  // 温度を取得してシリアル出力します。
  Serial.print("Temperature = ");
  temp=bme.readTemperature();
  Serial.print(temp);
  Serial.println(" *C");
  // 湿度を取得してシリアル出力します。
  Serial.print("Pressure = ");
  pressure=bme.readPressure()/ 100.0F;
  Serial.print(pressure);
  Serial.println(" hPa");
  // 気圧を取得してシリアル出力します。
  Serial.print("Humidity = ");
  humidity=bme.readHumidity();
  Serial.print(humidity);
  Serial.println(" %");
  // バッテリー電圧を取得します。
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, LOW);
  //なぜか一度読みだしてから再度読みだした方がデータ精度がよいので、一度ダミー読み出しをする。
  int vbat_raw = analogRead(PIN_VBAT);
  //こちらが本利用データ
  vbat=(float)analogRead(PIN_VBAT)/1024 * 2.4 / 510 * 1510;
  digitalWrite(VBAT_ENABLE, HIGH);
  Serial.print("Vbat = ");
  //Serial.println(vbat_mv);
  Serial.println(vbat);

  int var;
  var = temp*100;
  #define offset 7
  serviceData[offset+1] = var >>8;
  serviceData[offset+2] = var & 0xFF;
  var = humidity*100;
  serviceData[offset+4] = var >>8;
  serviceData[offset+5] = var & 0xFF;
  var = pressure*10;
  serviceData[offset+7] = var >>8;
  serviceData[offset+8] = var & 0xFF;
  var = vbat*10;
  serviceData[offset+10] = var & 0xFF;

#ifdef CO2

  //UARTでCO2データ取得してシリアル出力します。
  Serial1.write(ReadCO2,sizeof ReadCO2);
  Serial1.readBytes((char *)retval, sizeof retval);
  uartco2 = retval[2]*256 + retval[3];
  Serial.print("CO2 = ");
  Serial.print(uartco2);
  Serial.println();

  serviceData[offset+12] = uartco2 >>8;
  serviceData[offset+13] = uartco2 & 0xFF;

#endif

  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, serviceData, sizeof(serviceData));

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(100, 100);    // in units of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(1);      // number of seconds in fast mode
  Bluefruit.Advertising.start(1);      // Stop advertising entirely after ADV_TIMEOUT seconds

  Serial.println("Advertising is started");

  // 緑LEDを一瞬光らせます。
  /*
  digitalWrite(LED_GREEN, LOW);
  delay(100);
  digitalWrite(LED_GREEN, HIGH);
  */
}

void setup() {

  // LEDの制御を取り戻します。
  Bluefruit.autoConnLed(false);

  // 状態表示用にLEDピンを初期化します。
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);

  Serial.begin(9600);

/*
  while(!Serial){
      // シリアル待ちの間青色LEDを点滅します。
      digitalWrite(LED_BLUE, LOW);
      delay(500);
      digitalWrite(LED_BLUE, HIGH);
      delay(500);
  }
*/
  // BME280の初期化です。
  Serial.println(F("BME280 initialization"));

  if (! bme.begin(0x76, &Wire)) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      while (1){
        // センサーが接続されていない場合は赤色LEDを点滅します。
        digitalWrite(LED_RED, LOW);
        delay(500);
        digitalWrite(LED_RED, HIGH);
        delay(500);
      }
  }

  //Serial.println("-- Default Test --");
  //Serial.println("normal mode, 16x oversampling for all, filter off,");
  //Serial.println("0.5ms standby period");

  // weather monitoring
  Serial.println("-- Weather Station Scenario --");
  Serial.println("forced mode, 1x temperature / 1x humidity / 1x pressure oversampling,");
  Serial.println("filter off");
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );


#ifdef CO2

  // CO2センサの初期化です。
  Serial1.begin(9600);
  Serial1.write(SelfCalOn,sizeof SelfCalOn);

#endif

  // Enable DC-DC converter
  NRF_POWER->DCDCEN = 1;  // Enable DC/DC converter for REG1 stage

  // Sleep QSPI flash
  flashTransport.begin();
  flashTransport.runCommand(0xB9);  // enter deep power-down mode
  flashTransport.end();

  //念のため少しウエイトを入れます。
  delay(2000);

  // BLE処理を開始します。
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values

  // BLEのMACアドレス表示用変数
  char address[20];
  // BLEのMACアドレスを取得します。
  ble_gap_addr_t blead;
  blead = Bluefruit.getAddr();
  sprintf(address,"BLE address : %x:%x:%x:%x:%x:%x",blead.addr[0],blead.addr[1],blead.addr[2],blead.addr[3],blead.addr[4],blead.addr[5]);
  Serial.println(address);

  // BLE MACアドレスの下4バイトをムセンコネクトさんの個体識別番号に設定します。
  serviceData[3]=blead.addr[2];
  serviceData[4]=blead.addr[3];
  serviceData[5]=blead.addr[4];
  serviceData[6]=blead.addr[5];

  //WDT
  NRF_WDT->CONFIG         = 0x01;     // Configure WDT to run when CPU is asleep
  NRF_WDT->CRV            = 9830401;  // Timeout set to 300 seconds, timeout[s] = (CRV-1)/32768
  NRF_WDT->RREN           = 0x01;     // Enable the RR[0] reload register
  NRF_WDT->TASKS_START    = 1;        // Start WDT

  // For Battery management
  //pinMode(HICHG, OUTPUT);
  //digitalWrite(HICHG, HIGH); // Low Charging Current
//digitalWrite(HICHG, LOW);  // High Charging Current

  analogReference(AR_INTERNAL_2_4); // VREF = 2.4V
  analogReadResolution(10);         // 10bit A/D



  // ループの間隔設定用、10秒間隔の取得にしています。
  delayTime = 10000; // in milliseconds


}

void loop() {

  // センサから値を取得して、アドバタイズします。
  startAdv();

  //待ち時間を入れます。
  delay(delayTime);

  Bluefruit.Advertising.clearData();  // refresh advertised data for each cycle

  // WDT処理です。
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;

}
