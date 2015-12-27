/*
******** Control Scheme ********

ModeBtn:
- Hold: Starts edit mode.
- Press: Switches to next display mode. Switches to next field in edit mode.

UpBtn:
- Hold: Turns alarm on/off.
- Press: Switches between 12h/24h formats. Increases value in edit mode.

DownBtn:
- Press: Decreases value in edit mode.

Combinations:
Hold UpBtn + Hold DownBtn = Reset

Notes:

- Sometimes the memory is not loaded correctly with the 2D array immediately after uploading. Pushing the reset button on the arduino fixes the issue.
- The contrast control pin must have a capacitor (~=10uF) connected to ground to regulate the output voltage and reduce lcd flickering.

*/
#include <LiquidCrystal.h>
#include <EEPROM.h>

typedef unsigned short ushort;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);
const ushort SCREENWIDTH = 16;

// Custom digit pixel data (Source: https://gist.github.com/ronivaldo/5587355 )
unsigned char cells[8][8] = {
  {B11100, B11110, B11110, B11110, B11110, B11110, B11110, B11100},
  {B00111, B01111, B01111, B01111, B01111, B01111, B01111, B00111},
  {B11111, B11111, B00000, B00000, B00000, B00000, B11111, B11111},
  {B11110, B11100, B00000, B00000, B00000, B00000, B11000, B11100},
  {B01111, B00111, B00000, B00000, B00000, B00000, B00011, B00111},
  {B00000, B00000, B00000, B00000, B00000, B00000, B11111, B11111},
  {B00000, B00000, B00000, B00000, B00000, B00000, B00111, B01111},
  {B11111, B11111, B00000, B00000, B00000, B00000, B00000, B00000}
};
////////////////////////////////////////////////////////////////////////////////
unsigned char digitmap[10][6] =
{
  {2, 8, 1, 2, 6, 1}, {' ', 1, ' ', ' ', 1, ' '}, {5, 3, 1, 2, 6, 6}, {5, 3, 1, 7, 6, 1}, {2, 6, 1, ' ', ' ', 1}, {2, 3, 4, 7, 6, 1}, {2, 3, 4, 2, 6, 1}, {2, 8, 1, ' ', ' ', 1}, {2, 3, 1, 2, 6, 1}, {2, 3, 1, 7, 6, 1}
};
#define buttoncount 3
#define defdelay 100
#define holdtime 1000
#define buzzer 11
#define modebtn 0
#define upbtn 1
#define downbtn 2
#define stclock 0
#define stalarm 1
#define stsettings 2
#define statecount 3
#define editstatecount 2
#define alarmtimeout 30000 // This value will be ignored if larger than 60000
#define lighttimeout 30000
#define backlightpin 10
#define contrastpin 9
#define defcontrast 50
#define defbrightness 150
#define maxcontrast 128
#define maxbrightness 255
#define F0 5
#define F1 6
unsigned char buttons[buttoncount] = {A5, A4, A3};
ushort buttontime[buttoncount] = {0, 0, 0};
bool buttonholdstate[buttoncount] = {0, 0, 0};
// Program
ushort state = stclock;
bool editing = false, alarmon = false, is12hr = false, alarmset = false, supressring = false, statechanged = true, lightison = false;
int altrigtime = 0, lt = 0, lighttrigtime = 0, contrast = 80, brightness = 150;
ushort estate = 0;
ushort h = 0, m = 0, s = 0, alh = 0, alm = 0;
void reset()
{
  s = m = h = alm = alh = 0; statechanged = true; alarmset = alarmon = supressring = false;
  contrast = defcontrast;
  brightness = defbrightness;
  updatelcdsettings();
  savealarm();
  savesettings();
}
void lighton()
{
  lighttrigtime = millis();
  lightison = true;
  analogWrite(backlightpin, brightness);
}
void lightoff()
{
  lightison = false;
  digitalWrite(backlightpin, LOW);
}
void savesettings()
{
  EEPROM.write(3, contrast);
  EEPROM.write(4, brightness);
}
void loadsettings()
{
  contrast = EEPROM.read(3) % (maxcontrast + 1);
  brightness = EEPROM.read(4) % (maxbrightness + 1);
}
void savealarm()
{
  EEPROM.write(0, alm);
  EEPROM.write(1, alh);
  EEPROM.write(2, alarmset);
}
void loadalarm()
{
  alm = EEPROM.read(0) % 60;
  alh = EEPROM.read(1) % 24;
  alarmset = EEPROM.read(2);
}
void updatelcdsettings()
{
  analogWrite(contrastpin, contrast);
  analogWrite(backlightpin, brightness);
}
float lasttemp = 25;
void updatetemp()
{
  // Temp
  //digitalWrite(A1, HIGH);
  float val = analogRead(A0);
  //digitalWrite(A1, LOW);
  float mv = ( val / 1024.0) * 5000;
  float cel = mv / 10;
  // Minimize the effect of sudden fluctuations
  lasttemp = lasttemp * 3 + cel;
  lasttemp /= 4;
  lcd.setCursor(14, 0);
  lcd.print((int)lasttemp);
}
void setup() {
  for (ushort i = 0; i < 8; i++)
    lcd.createChar(i + 1 /*Cannot be 0*/, cells[i]);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  for (ushort i = 0; i < buttoncount; i++)
    pinMode(buttons[i], INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(A0, INPUT); // Temp read
  pinMode(A1, OUTPUT); // Alternative VCC for temp sensor
  pinMode(backlightpin, OUTPUT); // Backlight
  pinMode(contrastpin, OUTPUT);
  if (!(EEPROM.read(F0)^EEPROM.read(F1))) // First time to run
  {
    EEPROM.write(F0, !EEPROM.read(F0)); // Invert
    reset();
  }
  updatetemp();
  loadalarm();
  loadsettings();
  lighton();
  analogWrite(contrastpin, contrast);
  digitalWrite(A1, HIGH);
  Serial.begin(9600);
}
void clockpulse()// 1sec
{
  s++;
  if (s >= 60)
  {
    s = 0;
    m++;
    statechanged = true;
  }
  if (m >= 60)
  {
    m = 0;
    h++;
    statechanged = true;
  }
  if (h >= 24)
    h = 0;
  if (s % 2) // Update temperature every 2 seconds
    updatetemp();
  lcd.setCursor(14, 1);
  lcd.write(' ');
  lcd.setCursor(13, 1);
  lcd.print(s);
  lt--; // This 1ms per second leap fine tunes the clock
}
void buttonhold(int i)
{
  lighton();
  if (i == modebtn)
  {
    editing = !editing;
    if (!editing)
      if (state == stalarm)
      {
        alarmset = true;
        savealarm();
      }
      else if (state == stclock)
        s = 0;
      else if (state == stsettings)
      {
        savesettings();
      }
    estate = 0;
    statechanged = true;
  }
  else if (i == upbtn)
  {
    if (buttontime[downbtn] > 0)
    {
      reset();
    }
    else if (!editing)
    {
      alarmset = !alarmset;
      alarmon &= alarmset;
      digitalWrite(buzzer, LOW);
      savealarm();
      statechanged = true;
    }
  }
}
void buttonpress(int i)
{
  if (i == modebtn && !lightison) // Mode button will turn the light on with no effect
  {
    lighton();
    return;
  }
  lighton(); // Any button press should reset the light cycle
  if (alarmon)
  {
    if (i == modebtn)
    {
      alarmon = false;
      supressring = true;
      digitalWrite(buzzer, LOW);
    }
  }
  else
  {
    if (editing)
    {
      if (i == modebtn)
      {
        estate++;
        estate %= editstatecount;
      }
    }
    else
    {
      if (i == modebtn)
      {
        state++;
        state %= statecount;
        statechanged = true;
      }
      else if (i == upbtn || i == downbtn)
      {
        is12hr = !is12hr;
        statechanged = true;
      }
    }
  }
}
void checkinput()
{
  for (ushort i = 0; i < buttoncount; i++)
  {
    if (digitalRead(buttons[i]) == HIGH)
    {
      if (buttontime[i] > 0)
      {
        buttontime[i] += defdelay;
        if (buttontime[i] >= holdtime && !buttonholdstate[i])
        {
          buttonholdstate[i] = true;
          buttonhold(i);
        }
      }
      else
      {
        buttontime[i] = defdelay / 2;
        //buttondown(i);
      }
    }
    else
    {
      if (buttontime[i] > 0)
      {
        buttontime[i] = 0;
        if (!buttonholdstate[i])
          buttonpress(i);
        buttonholdstate[i] = false;
        //buttonup(i);
      }
    }
  }
}
// The row parameter is unnecessary for n*2 lcds
void printdigit(unsigned short digit, unsigned short col, unsigned short row)
{
  digit %= 10;
  ushort i = 0;
  lcd.setCursor(col, row);
  for (; i < 3; i++)
    lcd.write(digitmap[digit][i]);
  lcd.setCursor(col, row + 1);
  for (; i < 6; i++)
    lcd.write(digitmap[digit][i]);
}
void printushort(unsigned short num, unsigned short col, unsigned short row, unsigned short mindig)
{
  if (num < 10) // Print digits quickly
  {
    while (mindig-- > 1 && col < SCREENWIDTH)
    {
      printdigit(0, col, row);
      col += 3;
    }
    printdigit(num, col, row);
    return;
  }
  ushort c = 0;
  ushort dc = SCREENWIDTH / 3;
  ushort digits[dc];
  while (num > 0 && c < dc)
  {
    digits[c++] = num % 10;
    num /= 10;
  }
  while (c < mindig)
  {
    digits[c++] = 0;
  }
  while (c > 0 && col < SCREENWIDTH)
  {
    printdigit(digits[--c], col, row);
    col += 3;
  }
}
void clear(unsigned short col, unsigned short row, unsigned short w, unsigned short h)
{
  for (ushort x = 0; x < w; x++)
    for (ushort y = 0; y < h; y++)
    {
      lcd.setCursor(col + x, row + y);
      lcd.write(' ');
    }
}
ushort cc = 0;
#define cps 10
void loop() {
  lt = millis();
  cc++;
  while (cc >= cps) // cc can get larger than cps when increased by the latency compensation at the end of the last cycle.
  {
    clockpulse();
    cc -= cps;
  }
  checkinput();
  // Backlight
  if (lightison && lt - lighttrigtime > lighttimeout && !alarmon)
  {
    editing = false;
    state = stclock;
    statechanged = true;
    lightoff();
  }
  // Alarm handling
  if (!editing && alarmset) {
    if (alh == h && alm == m)
    {
      if (!alarmon && !supressring)
      {
        alarmon = true;
        lighton();
        altrigtime = lt;
      }
    }
    else if (supressring) // Supress periode is over
      supressring = false;
  }
  if (alarmon)
  {
    if (lt - altrigtime < alarmtimeout)
    {
      if (cc % 2)
        digitalWrite(buzzer, HIGH);
      else
        digitalWrite(buzzer, LOW);
    } else {
      alarmon = false;
      supressring = true;
      digitalWrite(buzzer, LOW);
    }
  }
  // 2 dots for sec counting
  if (state == stclock && !editing)
  {
    lcd.setCursor(6, 0);
    lcd.write(cc < cps / 2 ? 6 : ' ');
    lcd.setCursor(6, 1);
    lcd.write(cc < cps / 2 ? 8 : ' ');
  }
  // Only update the rest of the lcd if something was changed
  if (statechanged)
  {
    statechanged = editing;
    if (state == stclock)
    {
      if (editing)
      {
        if (buttontime[upbtn] > 0)
        {
          if (estate == 0)m++;
          else h++;
          m %= 60; h %= 24;
        }
        else if (buttontime[downbtn] > 0)
        {
            if (estate == 0)if (m == 0)m = 59; else m--;
          else if (h == 0)h = 23; else h--;
        }
        if (estate == 0 || cc % 2)
          printushort(h % (is12hr ? 12 : 24), 0, 0, 2);
        else
          clear(0, 0, 6, 3);
        if (estate == 1 || cc % 2)
          printushort(m, 7, 0, 2);
        else
          clear(7, 0, 6, 3);
        lcd.setCursor(6, 0);
        lcd.write(6);
        lcd.setCursor(6, 1);
        lcd.write(8);
      }
      else
      {
        printushort(h % (is12hr ? 12 : 24), 0, 0, 2);
        printushort(m, 7, 0, 2);
      }
      lcd.setCursor(13, 0);
      lcd.write(is12hr ? (h > 11 ? 'P' : 'A') : ' ');
    }
    else if (state == stalarm)
    {
      if (editing)
      {
        if (buttontime[upbtn] > 0)
        {
          if (estate == 0)alm++;
          else alh++;
          alm %= 60; alh %= 24;
        }
        else if (buttontime[downbtn] > 0)
        {
            if (estate == 0)if (alm == 0)alm = 59; else alm--;
          else if (alh == 0)alh = 23; else alh--;
        }
        if (estate == 0 || cc % 2)
          printushort(alh % (is12hr ? 12 : 24), 0, 0, 2);
        else
          clear(0, 0, 6, 3);
        if (estate == 1 || cc % 2)
          printushort(alm, 7, 0, 2);
        else
          clear(7, 0, 6, 3);
      }
      else
      {
        printushort(alh % (is12hr ? 12 : 24), 0, 0, 2);
        printushort(alm, 7, 0, 2);
      }
      lcd.setCursor(6, 0);
      lcd.write(6);
      lcd.setCursor(6, 1);
      lcd.write(8);
      lcd.setCursor(13, 0);
      lcd.write(is12hr ? (alh > 11 ? 'P' : 'A') : ' ');
    }
    else if (state == stsettings)
    {
      if (editing)
      {
        if (buttontime[upbtn] > 0)
        {
          if (estate == 0)contrast++;
          else brightness++;
          contrast %= maxcontrast + 1; brightness %= maxbrightness + 1;
        }
        else if (buttontime[downbtn] > 0)
        {
            if (estate == 0)if (contrast == 0)contrast = maxcontrast; else contrast--;
          else if (brightness == 0)brightness = maxbrightness; else brightness--;
        }
        updatelcdsettings();
        lcd.setCursor(0, 0);
        lcd.print("Contrast ");
        if (estate == 1 || cc % 2)
        {
          lcd.print(contrast);
        } else
          lcd.print("   ");
        lcd.setCursor(0, 1);
        lcd.print("Light ");
        if (estate == 0 || cc % 2)
        {
          lcd.print(brightness);
        } else
          lcd.print("   ");
      }
      else
      {
        clear(0, 0, 13, 2);
        lcd.setCursor(0, 0);
        lcd.print("Contrast ");
        lcd.print(contrast);
        lcd.setCursor(0, 1);
        lcd.print("Light ");
        lcd.print(brightness);
      }
    }
    lcd.setCursor(15, 1);
    lcd.write(alarmset ? '!' : ' ');
  }
  int offset = defdelay - (millis() - lt);
refloop: // Refine the delay to reduce accumulated error
  lt = millis();
  while (offset < 0)
  {
    offset += defdelay;
    cc++;
  }
  offset -= millis() - lt;
  if (offset < 0)goto refloop;
  delay(offset);
}
