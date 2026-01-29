#include <GyverWDT.h>
#include <Wire.h>

#define len(x) (sizeof(x) / sizeof(typeof(x)))

struct data_to_slave {
  char type;
  uint8_t state;
  char number[16];
};

struct sys_port {
  int addr;
  int number;
  int dial_port;
};

struct data_to_slave dts0 = { 0, 0, 0 };
struct sys_port p0 = { 0x33, 201, -1 };
struct data_to_slave dts1 = { 0, 0, 0 };
struct sys_port p1 = { 0x34, 202, -1 };
struct data_to_slave dts2 = { 0, 0, 0 };
struct sys_port p2 = { 0x35, 203, -1 };
struct data_to_slave dts3 = { 0, 0, 0 };
struct sys_port p3 = { 0x36, 204, -1 };
struct data_to_slave dts4 = { 0, 0, 0 };
struct sys_port p4 = { 0x37, 205, -1 };

struct sys_port devs[] = { p0, p1, p2, p3, p4 };
struct data_to_slave dtss[] = { dts0, dts1, dts2, dts3, dts4 };
int nDevices = 0;

int mks[5][5] = {
  { -1, 4, 5, 6, 7},
  {4, -1, 8, 9, 10},
  {5, 8, -1, 11, 12},
  {6, 9, 11, -1, 14},
  {7, 10, 12, 14, -1}
};

void connecting(int port1, int port2, bool discon = 0) {
  digitalWrite(mks[port1][port2], !discon);
}

void setup() {
  // Начинаем работу с I2C
  Wire.begin();
  Wire.setClock(100000);
  pinMode(13, OUTPUT);

  for (int p = 4; p < 15; p++) pinMode(p, OUTPUT);

  // Проверяем сколько абонентских модулей подключено
  for (byte address = 1; address < 128; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      nDevices++;
    }
  }

  if (nDevices == 0) {  // Если на шине нет ни одного абонентского к-та
    while (true) {
      digitalWrite(13, 1);
      delay(100);
      digitalWrite(13, 0);
      delay(500);
    }
  } else if (nDevices > (sizeof(devs) / sizeof(sys_port))) {
    while (true) {
      digitalWrite(13, 1);
      delay(100);
      digitalWrite(13, 0);
      delay(100);
      digitalWrite(13, 1);
      delay(100);
      digitalWrite(13, 0);
      delay(500);
    }
  }
  for (int j = 0; j < 5; j++) {
    Wire.beginTransmission(devs[j].addr);
    dtss[j].state = 1; // Включение абонентских комплектов
    dtss[j].type = '!';
    Wire.write((uint8_t*)&dtss[j], 18);
    Wire.endTransmission();
  }
  Watchdog.enable(RESET_MODE, WDT_PRESCALER_128);
}

void loop() {
  Watchdog.reset();
  for (int p = 0; p < 5; p++) {  // Опрос абонентских комплектов
    Wire.beginTransmission(devs[p].addr);
    dtss[p].type = '?';
    Wire.write((uint8_t*)&dtss[p], 18);
    uint8_t result = Wire.endTransmission();
    if (result != 0) continue;
    delay(5);

    Wire.requestFrom(devs[p].addr, 17);
    uint8_t i = 1;
    while (Wire.available() > 0) {
      *((uint8_t*)&dtss[p] + i++) = Wire.read();
    }

    if (dtss[p].state == 4) continue;

    if (dtss[p].state == 5) {  // Абонент набрал номер, ждёт подтверждение станции
      int number = String(dtss[p].number).toInt();
      bool equal = false;
      //Serial.println(number);
      if (number != devs[p].number) {
        for (int j = 0; j < 5; j++) {  // Поиск номера в базе
          if (number == devs[j].number) {
            if (dtss[j].state <= 1) {
              dtss[p].state = 6;  // Наш абонент получает статус 6

              dtss[j].state = 2;  // Вызываемый абонент получает статус 2
              Wire.beginTransmission(devs[j].addr);
              dtss[j].type = '!';
              Wire.write((uint8_t*)&dtss[j], 18);
              result = Wire.endTransmission();

              devs[p].dial_port = j;
              devs[j].dial_port = p;
              equal = true;
              break;
            }
          }
        }
      }
      if (!equal) dtss[p].state = 8;  // Если номер не прошёл валидацию, статус 8
    } else if (dtss[p].state == 6) {
      if (dtss[devs[p].dial_port].state == 7) {
        dtss[p].state = 7;
        connecting(p, devs[p].dial_port);
      }
    } else if (dtss[p].state == 7) {
      if (dtss[devs[p].dial_port].state == 1) {  // Абонент положил трубку во время разговора
        dtss[p].state = 8;
        connecting(p, devs[p].dial_port, true);
      }
    } else if (dtss[p].state == 1) {
      connecting(p, devs[p].dial_port, true);
      devs[p].dial_port = -1;
    } else if (dtss[p].state == 2) {
      if (dtss[devs[p].dial_port].state == 1) dtss[p].state = 1;
    }
    Wire.beginTransmission(devs[p].addr);
    dtss[p].type = '!';
    Wire.write((uint8_t*)&dtss[p], 18);
    result = Wire.endTransmission();
  }
  delay(50);
}
