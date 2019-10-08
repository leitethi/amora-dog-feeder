/* 
  Amora Dog Feeder v1.0
  Thiago Leite

  Connections
  RTC
  SCL - A5
  SDA - A4
  RELAY
  DIGITAL 13
*/

#include <LiquidCrystal.h>

#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

RTC_DS1307 rtc;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define RELE_PIN 13

short MENU_TIMEOUT_VALUE = 5000;
unsigned long timeoutValue = 0;

uint8_t bellIcon[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};

byte portionIcon[8] = {
  B00100,
  B00100,
  B01110,
  B01110,
  B11111,
  B11111,
  B11111,
  B01110
};

byte checkIcon[] = {
  B00000,
  B00001,
  B00011,
  B10110,
  B11100,
  B01000,
  B00000,
  B00000
};

byte currentMenu = 0;

byte MEMORY_ADDRESS_1 = 0;
byte MEMORY_ADDRESS_2 = 10;

uint32_t blinkInterval = 500;
uint32_t blinkPreviousMillis = 0;
uint32_t blinkCurrentMillis = 0;
boolean isBlinking  = false;
boolean executedAlarm1 = false;
boolean executedAlarm2 = false;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

unsigned short day = 0, month = 0, hour = 0, minute = 0, second = 0;
unsigned long year = 0;

char weekDays[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

enum LCDButton: byte {
  ButtonSelect = 5,
  ButtonRight = 4,
  ButtonLeft = 3,
  ButtonUp = 2,
  ButtonDown = 1,
  ButtonNone = 0
};

enum SCREEN_STATE: byte {
  MAIN,
  FEED_TIME1,
  FEED_TIME1_EDIT_HOUR,
  FEED_TIME1_EDIT_MINUTE,
  FEED_TIME1_EDIT_ONOFF,
  
  FEED_TIME2,
  FEED_TIME2_EDIT_HOUR,
  FEED_TIME2_EDIT_MINUTE,
  FEED_TIME2_EDIT_ONOFF,

  EDIT_DAY,
  EDIT_MONTH,
  EDIT_YEAR,
  
  PORTIONS,
  EDIT_PORTION1,
  EDIT_PORTION2,

  CLOCK,
  CLOCK_EDIT_MONTH,
  CLOCK_EDIT_DAY,
  CLOCK_EDIT_YEAR,
  CLOCK_EDIT_HOUR,
  CLOCK_EDIT_MINUTE
};


SCREEN_STATE state;

enum DatePart: int {
  DAY,
  MONTH,
  YEAR,
  HOUR,
  MINUTE,
  SECOND
};

struct Schedule {
  short hour;
  short minute;
  short second;
  bool enabled;
  bool didExecute;
};

Schedule schedule1 = { 0, 0, 0, false, false };
Schedule schedule2 = { 0, 0, 0, false, false };

void setup() {
  Serial.begin(9600);

  lcd.begin(16, 2);
  
  lcd.noDisplay();
  delay(500);
  // Turn on the display:
  lcd.display();
  delay(500);

  lcd.print("DogFeeder v 0.1");
  
  // delay(4000);
  lcd.clear();

  if (!rtc.begin()) {
    Serial.println("DS1307 NAO INICIALIZADO");
    while (1);
  }

  pinMode(RELE_PIN, OUTPUT);

  lcd.createChar(0, bellIcon);
  lcd.createChar(1, portionIcon);
  lcd.createChar(2, checkIcon);

  state = MAIN;

  loadAlarms();
}

void prefixWithZero(int value) {
  if (value < 10) {
    lcd.print("0");
  }

  lcd.print(value);
}

void changeState() {
  switch(state) {
    case MAIN:
      displayMAIN();
      break;
    case FEED_TIME1:
      displayFeedTime("Feed Time #1", schedule1);
      //displayFEED_TIME1();
      break;
    case FEED_TIME1_EDIT_HOUR:
      displayEditHourFeedTime(schedule1);
      break;
    case FEED_TIME1_EDIT_MINUTE:
      displayEditMinuteFeedTime(schedule1);
      break;
    case FEED_TIME1_EDIT_ONOFF:
      displayEditOnOffFeedTime(schedule1);
      break;
    case FEED_TIME2:
      displayFeedTime("Feed Time #2", schedule2);
      break;
    case FEED_TIME2_EDIT_HOUR:
      displayEditHourFeedTime(schedule2);
      break;
    case FEED_TIME2_EDIT_MINUTE:
      displayEditMinuteFeedTime(schedule2);
      break;
    case FEED_TIME2_EDIT_ONOFF:
      displayEditOnOffFeedTime(schedule2);
      break;
    case PORTIONS:
      displayPortions();
      break;
    case EDIT_PORTION1:
      displayEditPortion1();
      break;
    case EDIT_PORTION2:
      displayEditPortion2();
      break;
    case CLOCK:
      displayClock();
      break;
    case CLOCK_EDIT_MONTH:
      displayEditClockMonth();
      break;
    case CLOCK_EDIT_DAY:
      displayEditClockDay();
      break;
    case CLOCK_EDIT_YEAR:
      displayEditClockYear();
      break;
    case CLOCK_EDIT_MINUTE:
      displayEditClockMinute();
      break;
    case CLOCK_EDIT_HOUR:
      displayEditClockHour();
      break;
  }
}

void setClockValues(DateTime dateTime) {
  month = dateTime.month();
  day = dateTime.day();
  year = dateTime.year();
  hour = dateTime.hour();
  minute = dateTime.minute();
  second = dateTime.second();
}

void resetAlarms() {
  DateTime dateTime = rtc.now();

  if (dateTime.hour() == 0 && dateTime.minute() == 0) {
    executedAlarm1 = false;
    executedAlarm2 = false;
  }
}

void checkFeedTime() {
  DateTime dateTime = rtc.now();

  Serial.println(schedule1.didExecute);
  if (schedule1.hour == dateTime.hour() && schedule1.minute == dateTime.minute() && schedule1.didExecute == false) {
    Serial.println("alarm1");
    digitalWrite(RELE_PIN, HIGH);
    schedule1.didExecute = true;
    delay(schedule1.second * 1000);
    digitalWrite(RELE_PIN, LOW);
  } else if (schedule2.hour == dateTime.hour() && schedule2.minute == dateTime.minute() && schedule2.didExecute == false) {
    digitalWrite(RELE_PIN, HIGH);
    schedule2.didExecute = true;
    delay(schedule2.second * 1000);
    digitalWrite(RELE_PIN, LOW);
  }
}

void displayClock() {
  lcd.setCursor(1, 0);
  DateTime dateTime = rtc.now();

  setClockValues(dateTime);

  lcd.print(weekDays[dateTime.dayOfTheWeek()]);

  lcd.setCursor(5, 0);
  prefixWithZero(dateTime.month());
  lcd.print('/');
  prefixWithZero(dateTime.day());
  lcd.print('/');
  prefixWithZero(dateTime.year());
  
  lcd.setCursor(4,1);
  prefixWithZero(dateTime.hour());
  lcd.print(':');
  prefixWithZero(dateTime.minute());
  lcd.print(':');
  prefixWithZero(dateTime.second());
}

void displayEditClockMonth() {
  lcd.setCursor(3, 0);
  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(month);
  }
  lcd.print('/');
  prefixWithZero(day);
  lcd.print('/');
  prefixWithZero(year);
  
  lcd.setCursor(4,1);
  prefixWithZero(hour);
  lcd.print(':');
  prefixWithZero(minute);

  blinkCursor();
}

void displayEditClockDay() {
  lcd.setCursor(3, 0);
  prefixWithZero(month);
  
  lcd.print('/');
    if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(day);
  }
  
  lcd.print('/');
  prefixWithZero(year);
  
  lcd.setCursor(4,1);
  prefixWithZero(hour);
  lcd.print(':');
  prefixWithZero(minute);

  blinkCursor();
}

void displayEditClockYear() {
  lcd.setCursor(3, 0);
  prefixWithZero(month);
  
  lcd.print('/');
  prefixWithZero(day);
  
  lcd.print('/');
  if (isBlinking) {
    lcd.print("    ");
  } else {
    prefixWithZero(year);
  }
  
  lcd.setCursor(4,1);
  prefixWithZero(hour);
  lcd.print(':');
  prefixWithZero(minute);

  blinkCursor();
}

void displayEditClockHour() {
  lcd.setCursor(3, 0);
  prefixWithZero(month);
  
  lcd.print('/');
  prefixWithZero(day);
  
  lcd.print('/');
  prefixWithZero(year);
  
  lcd.setCursor(4,1);
  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(hour);
  }
  lcd.print(':');
  prefixWithZero(minute);

  blinkCursor();
}

void displayEditClockMinute() {
  lcd.setCursor(3, 0);
  prefixWithZero(month);
  
  lcd.print('/');
  prefixWithZero(day);
  
  lcd.print('/');
  prefixWithZero(year);
  
  lcd.setCursor(4,1);
  prefixWithZero(hour);

  lcd.print(':');
  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(minute);
  }

  blinkCursor();
}

void displayPortions() {
  lcd.setCursor(0, 0);
  lcd.print("Edit Portions");

  lcd.setCursor(0, 1);
  lcd.print("#1: ");
  lcd.print(schedule1.second);

  lcd.setCursor(7, 1);
  lcd.print("#2: ");
  lcd.print(schedule2.second);
}

void displayEditPortion1() {
  lcd.setCursor(0, 1);
  lcd.print("#1: ");
  
  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(schedule1.second);
  }

  lcd.setCursor(7, 1);
  lcd.print("#2: ");
  lcd.print(schedule2.second);

  blinkCursor();
}

void displayEditPortion2() {
  lcd.setCursor(0, 1);
  lcd.print("#1: ");
  prefixWithZero(schedule1.second);

  lcd.setCursor(7, 1);
  lcd.print("#2: ");
  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(schedule2.second);
  }

  blinkCursor();
}

void displayFeedTime(String title, Schedule schedule) {
  lcd.setCursor(0, 0);
  lcd.print(title);
  lcd.setCursor(0, 1);
  
  prefixWithZero(schedule.hour);
  lcd.print(":");
  prefixWithZero(schedule.minute);

  lcd.setCursor(7, 1);
  if (schedule.enabled) {
    lcd.print("ENABLED ");
  } else {
    lcd.print("DISABLED");
  }
}

void displayEditHourFeedTime(Schedule schedule) {
  lcd.setCursor(0, 1);

  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(schedule.hour);
  }
  
  lcd.print(":");
  prefixWithZero(schedule.minute);
  lcd.setCursor(7, 1);
  if (schedule.enabled) {
    lcd.print("ENABLED ");
  } else {
    lcd.print("DISABLED");
  }

  blinkCursor();
}

void displayEditMinuteFeedTime(Schedule schedule) {
  lcd.setCursor(0, 1);
  prefixWithZero(schedule.hour);

  lcd.setCursor(3, 1);

  if (isBlinking) {
    lcd.print("  ");
  } else {
    prefixWithZero(schedule.minute);
  }
  lcd.setCursor(7, 1);
  if (schedule.enabled) {
    lcd.print("ENABLED ");
  } else {
    lcd.print("DISABLED");
  }

  blinkCursor();
}

void displayEditOnOffFeedTime(Schedule schedule) {
  lcd.setCursor(0, 1);
  prefixWithZero(schedule.hour);

  lcd.setCursor(3, 1);
  prefixWithZero(schedule.minute);

  lcd.setCursor(7, 1);

  if (isBlinking) {
    lcd.print("        ");
  } else {
    if (schedule.enabled) {
      lcd.print("ENABLED ");
    } else {
      lcd.print("DISABLED");
    }
  }

  blinkCursor();
}

void displayMAIN() {
  displayTimeSchedule(schedule1, 0);
  displayTimeSchedule(schedule2, 1);

  DateTime dateTime = rtc.now();
  
  lcd.setCursor(11, 0);
  prefixWithZero(dateTime.hour());
  lcd.print(':');
  prefixWithZero(dateTime.minute());
}

Schedule loadAlarm(byte address) {
  Schedule schedule;

  EEPROM.get(address, schedule);

  if (schedule.hour > 23 || schedule.hour < 0) schedule.hour = 0;
  if (schedule.minute > 59 || schedule.minute < 0) schedule.minute = 0;
  if (schedule.second > 59 || schedule.second < 0) schedule.second = 0;
  if (schedule.enabled != 1 && schedule.enabled != 0) schedule.enabled = 0;
  
  schedule.didExecute = 0;

  return schedule;
}

void loadAlarms() {
  schedule1 = loadAlarm(MEMORY_ADDRESS_1);
  schedule2 = loadAlarm(MEMORY_ADDRESS_2);
}

void displayTimeSchedule(Schedule schedule, byte positionY) {
  lcd.setCursor(0, positionY);

  if (schedule.enabled) {
    lcd.write(byte(0));  
  } else {
    lcd.print(" ");
  }

  prefixWithZero(schedule.hour);
  lcd.print(":");
  prefixWithZero(schedule.minute);

  lcd.setCursor(7, positionY);
  lcd.write(byte(1));
  lcd.print(schedule.second);
}

void loop() {
  changeState();

  checkInputs();

  checkFeedTime();

  // displayCurrentTime();

  //Serial.println(button);

  //changeMainMenu(button);

  // lcd.noBlink();
  // delay(3000);

  // lcd.blink();
  // delay(3000);
}

void checkInputs() {
  //if ( millis() - timeoutValue > MENU_TIMEOUT_VALUE )
 // {
 //   userInput = trigTIMEOUT;
 //   transition(userInput);
  //}

  LCDButton keyPressed = readButton();

  switch (keyPressed) {
    case ButtonSelect:
    case ButtonRight:
    case ButtonLeft:
    case ButtonDown:
    case ButtonUp:
      transition(keyPressed);
      break;
  }
}

void transition(byte trigger) {
  switch(state) {
    case MAIN:
      timeoutValue = millis();

      if (trigger == ButtonSelect ||
        trigger == ButtonRight ||
        trigger == ButtonLeft ||
        trigger == ButtonDown ||
        trigger == ButtonUp) {
        lcd.clear();
        state = FEED_TIME1;
      }
      break;
    case FEED_TIME1:
      timeoutValue = millis();

      if (trigger == ButtonLeft) {
        lcd.clear();
        state = MAIN;
      } else if (trigger == ButtonRight) {
        lcd.clear();
        state = FEED_TIME2;
      } else if (trigger == ButtonSelect) {
        // lcd.clear();
        state = FEED_TIME1_EDIT_HOUR;
      }
      break;
    case FEED_TIME1_EDIT_HOUR:
      timeoutValue = millis();

      if (trigger == ButtonUp) {
        schedule1.hour++;
        if (schedule1.hour > 23) schedule1.hour = 0;
      } else if (trigger == ButtonDown) {
        schedule1.hour--;
        if (schedule1.hour < 0) schedule1.hour = 23;
      } else if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = FEED_TIME1_EDIT_MINUTE;
      }
      break;
    case FEED_TIME1_EDIT_MINUTE:
      timeoutValue = millis();

      if (trigger == ButtonUp) {
        schedule1.minute++;
        if (schedule1.minute > 59) schedule1.minute = 0;
      } else if (trigger == ButtonDown) {
        schedule1.minute--;
        if (schedule1.minute < 0) schedule1.minute = 59;
      } else if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = FEED_TIME1_EDIT_ONOFF;
      } else if (trigger == ButtonLeft) {
        state = FEED_TIME1_EDIT_HOUR;
      }
      break;
    case FEED_TIME1_EDIT_ONOFF:
      timeoutValue = millis();

      if (trigger == ButtonUp || trigger == ButtonDown) {
        schedule1.enabled = !schedule1.enabled;
      } else if (trigger == ButtonSelect) {
        EEPROM.put(MEMORY_ADDRESS_1, schedule1);
        loadAlarms();
        state = FEED_TIME1;
      } else if (trigger == ButtonLeft) {
        state = FEED_TIME1_EDIT_MINUTE;
      }
      break;
    case FEED_TIME2:
      timeoutValue = millis();

      if (trigger == ButtonLeft) {
        lcd.clear();
        state = FEED_TIME1;
      } else if (trigger == ButtonRight) {
        lcd.clear();
        state = PORTIONS;
      } else if (trigger == ButtonSelect) {
        // lcd.clear();
        state = FEED_TIME2_EDIT_HOUR;
      }
      break;
    case FEED_TIME2_EDIT_HOUR:
      timeoutValue = millis();

      if (trigger == ButtonUp) {
        schedule2.hour++;
        if (schedule2.hour > 23) schedule2.hour = 0;
      } else if (trigger == ButtonDown) {
        schedule2.hour--;
        if (schedule2.hour < 0) schedule2.hour = 23;
      } else if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = FEED_TIME2_EDIT_MINUTE;
      }
      break;
    case FEED_TIME2_EDIT_MINUTE:
      timeoutValue = millis();

      if (trigger == ButtonUp) {
        schedule2.minute++;
        if (schedule2.minute > 59) schedule2.minute = 0;
      } else if (trigger == ButtonDown) {
        schedule2.minute--;
        if (schedule2.minute < 0) schedule2.minute = 59;
      } else if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = FEED_TIME2_EDIT_ONOFF;
      } else if (trigger == ButtonLeft) {
        state = FEED_TIME2_EDIT_HOUR;
      }
      break;
    case FEED_TIME2_EDIT_ONOFF:
      timeoutValue = millis();

      if (trigger == ButtonUp || trigger == ButtonDown) {
        schedule2.enabled = !schedule2.enabled;
      } else if (trigger == ButtonSelect) {
        EEPROM.put(MEMORY_ADDRESS_2, schedule2);
        loadAlarms();
        state = FEED_TIME2;
      } else if (trigger == ButtonLeft) {
        state = FEED_TIME2_EDIT_MINUTE;
      }
      break;
    case PORTIONS:
      timeoutValue = millis();

      if (trigger == ButtonSelect) {
        lcd.clear();
        state = EDIT_PORTION1;
      } else if (trigger == ButtonLeft) {
        lcd.clear();
        state = FEED_TIME2;
      } else if (trigger == ButtonRight) {
        lcd.clear();
        state = CLOCK;
      }
      break;
    case EDIT_PORTION1:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = EDIT_PORTION2;
      } else if (trigger == ButtonUp) {
        schedule1.second++;
        if (schedule1.second > 23) schedule1.second = 0;
      } else if (trigger == ButtonDown) {
        schedule1.second--;
        if (schedule1.second < 0) schedule1.second = 23;
      }
      break;
    case EDIT_PORTION2:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        EEPROM.put(MEMORY_ADDRESS_1, schedule1);
        EEPROM.put(MEMORY_ADDRESS_2, schedule2);
        loadAlarms();
        state = PORTIONS;
      } else if (trigger == ButtonLeft) {
        state = EDIT_PORTION1;
      } else if (trigger == ButtonUp) {
        schedule2.second++;
        if (schedule2.second > 23) schedule2.second = 0;
      } else if (trigger == ButtonDown) {
        schedule2.second--;
        if (schedule2.second < 0) schedule2.second = 23;
      }
      break;
    case CLOCK:
      if (trigger == ButtonSelect) {
        lcd.clear();
        state = CLOCK_EDIT_MONTH;
      } else if (trigger == ButtonLeft) {
        lcd.clear();
        state = PORTIONS;
      }
      break;
    case CLOCK_EDIT_MONTH:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = CLOCK_EDIT_DAY;
      } else if (trigger == ButtonUp) {
        month++;
        if (month > 12) month = 1;
      } else if (trigger == ButtonDown) {
        month--;
        if (month < 1) month = 12;
      }
      break;
    case CLOCK_EDIT_DAY:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = CLOCK_EDIT_YEAR;
      } else if (trigger == ButtonLeft) {
        state = CLOCK_EDIT_MONTH;
      } else if (trigger == ButtonUp) {
        day++;
        if (day > 31) day = 1;
      } else if (trigger == ButtonDown) {
        day--;
        if (day < 1) day = 31;
      }
      break;
    case CLOCK_EDIT_YEAR:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = CLOCK_EDIT_HOUR;
      } else if (trigger == ButtonLeft) {
        state = CLOCK_EDIT_DAY;
      } else if (trigger == ButtonUp) {
        year++;
      } else if (trigger == ButtonDown) {
        year--;
      }
      break;
    case CLOCK_EDIT_HOUR:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        state = CLOCK_EDIT_MINUTE;
      } else if (trigger == ButtonLeft) {
        state = CLOCK_EDIT_YEAR;
      } else if (trigger == ButtonUp) {
        hour++;
        if (hour > 23) hour = 1;
      } else if (trigger == ButtonDown) {
        hour--;
        if (hour < 1) hour = 23;
      }
      break;
    case CLOCK_EDIT_MINUTE:
      if (trigger == ButtonSelect || trigger == ButtonRight) {
        rtc.adjust(DateTime(year, month, day, hour, minute, 00));
        lcd.clear();
        state = CLOCK;
      } else if (trigger == ButtonLeft) {
        state = CLOCK_EDIT_HOUR;
      } else if (trigger == ButtonUp) {
        minute++;
        if (minute > 59) minute = 1;
      } else if (trigger == ButtonDown) {
        minute--;
        if (minute < 0) minute = 59;
      }
      break;
  }
}

void blinkCursor()
{
  blinkCurrentMillis = millis();

  if (blinkCurrentMillis - blinkPreviousMillis > blinkInterval)
  {
    blinkPreviousMillis = blinkCurrentMillis;

    isBlinking = !isBlinking;
  }
}

unsigned long keyTimeLast = 0;

LCDButton readButton()
{
    LCDButton keyLast = ButtonNone;

    int analogKey;
    LCDButton key;

    if (millis() < keyTimeLast) return ButtonNone;

    analogKey = analogRead(0);
    if (analogKey < 80) {
        key = ButtonRight;
    } else if (analogKey < 200) {
        key = ButtonUp;
    } else if (analogKey < 400) {
        key = ButtonDown;
    } else if (analogKey < 600) {
        key = ButtonLeft;
    } else if (analogKey < 800) {
        key = ButtonSelect;
    } else {
      key = ButtonNone;
    }

    if (analogKey != ButtonNone)
      keyTimeLast = millis() + (key == keyLast ? 100 : 1000);

    keyLast = key;
    
    return key;
}
