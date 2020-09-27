/*
 * EasyBitx Dual Band
 * - Noli Rafallo
 *      4G1ZYS
 *      nrafallo@gmail.com
 *      
*/

#include <LiquidCrystal.h>
#include <MD_REncoder.h>
#include <MD_KeySwitch.h>
#include <EEPROM.h>
#include <Wire.h>
#include <si5351mcu.h>

// LCD Display -----------------------------
// LCD display definitions
#define  LCD_ROWS  2
#define  LCD_COLS  16

// LCD pin definitions 
#define  LCD_RS    8
#define  LCD_ENA   9
#define  LCD_D4    10
#define  LCD_D5    LCD_D4+1
#define  LCD_D6    LCD_D4+2
#define  LCD_D7    LCD_D4+3

#define ENABLE_SPEED 1 // if sant to enable velocity tuning wherein the faster the knob rotation is the faster the frequency changes

static LiquidCrystal lcd(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Rotary Encoder and KeySwitch Setup ------
const uint8_t RE_A_PIN = 2;
const uint8_t RE_B_PIN = 3;
const uint8_t CTL_PIN = 4;

MD_REncoder  RE(RE_A_PIN, RE_B_PIN);
MD_KeySwitch swCtl(CTL_PIN);

const uint8_t LED_PIN = 13;

// Si5351 Setup ---------------------------

//Si5351 si5351;
Si5351mcu Si;

// Raduino Parameters ---------------------
// tuning range parameters

// USB/LSB parameters
#define CAL_VALUE 170000UL        // VFO calibration value
#define OFFSET_USB 1500           // USB offset in Hz [accepted range -10000Hz to 10000Hz]
#define VFO_DRIVE_LSB 4           // VFO drive level in LSB mod in mA [accepted values 2,4,6,8 mA]
#define VFO_DRIVE_USB 8           // VFO drive level in USB mod in mA [accepted values 2,4,6,8 mA]

// CW parameters
#define CW_SHIFT 500              // RX shift in CW mod in Hz, equal to sidetone pitch [accepted range 200-1200 Hz]

// mods
#define LSB (0)
#define USB (1)
#define CWL (2)
#define CWU (3)

// available PINS 5,6,7,22,A0, A1, A3
#define BAND_PIN (5)
#define DEFAULTBAND 0 // this is base on the order you define in the variables below

// User variables
unsigned long frequency;
unsigned long fStep = 100UL;
int memSaved = 1;

int8_t RIT = 0;
int ritmode = 0;
int stepmode = 0;
int vfomode = 0;

char c[17], b[10], printBuff[2][17];
bool locked = false;

// Menu var
const int topMenuCnt = 2;
const int subMenuCnt = 6;
const int ifMenuCnt = 2;
const int bandCnt = 2;
const char *band[2] = {"40m", "20m"};
const char *topMenuList[] = { "Main", "BFO"};
const char *subMenuList[] = { "Freq", "Mode", "Band", "Step", "RIT", "VFO" };
const char *ifMenuList[] = {"Band", "Freq"};
const char *trModes[] = { "LSB", "USB", "CWL", "CWU" };
const char *stpStr[] = { " 10", "100", " 1K", "10K" };
const unsigned int stpModes[] = { 10, 100, 1000, 10000 };
const unsigned long bandLimits[2][2] = {{7000000UL, 7800000UL}, {14000000UL, 14600000UL}};

int topMenuCtr = 0;
int subMenuCtr = 0;
int ifMenuCtr = 0;

struct userparameters {
  int USB_OFFSET = OFFSET_USB;
  unsigned long cal = CAL_VALUE;
  int mod = LSB;
  int stepmode = 1;
  int ritmode = 0;
  int vfomode = 0;
  int8_t RIT = 0;
  unsigned long bfo[2] = {10001500UL, 9997469UL};
  unsigned long vfo[2][2] = {{7000000UL, 7800000UL}, {14000000UL, 14600000UL}};;
  int currentBand = 0;
  int bfoBand = 0;
};

struct userparameters u;

void setupNav(void)
{
  RE.begin();
  swCtl.begin();
  swCtl.enableRepeat(false);
}


void setFrequency(unsigned long f) {

  unsigned long vfo_freq = 0;
  unsigned long tbfo = 0;
  if (!ritmode)
    RIT = 0;

  switch (u.mod) {
    case LSB:
      tbfo = u.bfo[u.mod];
      vfo_freq = f - tbfo + RIT;
      break;
    case USB:
      tbfo = u.bfo[u.mod];
      vfo_freq = f + tbfo + RIT;
      break;
    case CWL:
      tbfo = u.bfo[u.mod-2];
      vfo_freq = f - tbfo - CW_SHIFT + RIT;
      break;
    case CWU:
      tbfo = u.bfo[u.mod-2];
      vfo_freq = f + tbfo + CW_SHIFT + RIT;
      break;
    default:
      vfo_freq = f - u.bfo[u.mod] + RIT;
      u.mod = LSB;
      break;
  }
  Si.setFreq(0, vfo_freq + RIT);
  Si.setFreq(2, tbfo);
  Si.enable(0);
  Si.enable(2);

  if (topMenuList[topMenuCtr] == "Main") {
    mainDisplay(true);
  }
  memSaved = 0;
}

void mainDisplay(bool refresh) {
  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa(u.vfo[u.currentBand][u.vfomode], b, DEC);
  strcpy(c, "");

  if (!u.vfomode) {
    strcpy(c, "A");
  } else {
    strcpy(c, "B");
  }

  byte p = strlen(b);
  for (byte i = strlen(c); i < (2+(8-p)); i++) {
    strcat(c, " ");
  }  

  strncat(c, &b[0], p - 6);
  strcat(c, ",");
  strncat(c, &b[p - 6], 3);
  strcat(c, ".");
  strncat(c, &b[p - 3], 2);

  strcat(c, "  ");
  strcat(c, trModes[u.mod]);

  c[16] = '\0';
  printLine(0, c, refresh);

  // Print on line 2
  ultoa((fStep), b, DEC);

  strcpy(c, band[u.currentBand]); // display the band we are currently in
  
  for (byte i = strlen(c); i < 5; i++) {
    strcat(c, " ");
  }

  strcat(c, "S "); // display steps per tick of the encoder
  strcat(c, stpStr[stepmode]);

  for (byte i = strlen(c); i < 11; i++) {
    strcat(c, " ");
  }
  
  char rit[5];
  if (ritmode) {
    sprintf(rit, "%3i", RIT);  

    strcat(c, "R ");
    strcat(c, rit);
  }
  
  c[16] = '\0';
  printLine(1, c, refresh);
}

void updateIFDisplay(bool refresh) {
  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  strcpy(c, "BFO Frequency");

  c[16] = '\0';
  printLine(0, c, refresh);

  // Print on line 2
  ultoa(u.bfo[u.bfoBand], b, DEC);

  byte p = strlen(b);

  strcpy(c, trModes[u.bfoBand]);

  for (byte i = strlen(c); i < 4; i++) {
    strcat(c, " ");
  }

  strncat(c, &b[0], p - 6);
  strcat(c, ",");
  strncat(c, &b[p - 6], 3);
  strcat(c, ".");
  strncat(c, &b[p - 3], 3);

  c[16] = '\0';
  printLine(1, c, refresh);
  memSaved = 0;
}

void printLine(char linenmbr, char *c, bool refresh) {
  if (strcmp(c, printBuff[linenmbr]) || refresh) {
    lcd.setCursor(0, linenmbr);
    lcd.print(c);
    strcpy(printBuff[linenmbr], c);

    for (byte i = strlen(c); i < 16; i++) {
      lcd.print(' ');
    }
  }
}

void chMainMenu() {

  int col;

  topMenuCtr += 1;

  if (topMenuCtr >= topMenuCnt) topMenuCtr = 0; 
  
  if (topMenuList[topMenuCtr] == "Main") {
    subMenuCtr = subMenuCtr == 0 ? 4 : 0;
    col = subMenuCtr == 0 ? 3 : 0;
    ritmode = 0;
    lcd.setCursor(col,0);
    lcd.cursor();
    //mainMenu(0, 0);
    mainDisplay(true);
  }

  if (topMenuList[topMenuCtr] == "BFO") {
    lcd.cursor();
    ifMenu(0, 0);    
  }
  // lcd.print()
}

void chSubMenu() {

  if (topMenuList[topMenuCtr] == "Main") {
    
    subMenuCtr += 1;
    if (subMenuCtr >= subMenuCnt) subMenuCtr = 0; 
  
    lcd.cursor();
  
    switch(subMenuCtr) {
      case 0: lcd.setCursor(3, 0); break; 
      case 1: lcd.setCursor(13, 0); break;
      case 2: lcd.setCursor(0, 1); break;
      case 3: lcd.setCursor(5, 1); break;
      case 4: lcd.setCursor(11, 1); break;
      case 5: lcd.setCursor(0, 0); break;
      default: lcd.noCursor(); break;
    }
  }

  if (topMenuList[topMenuCtr] == "BFO") {
    
    ifMenuCtr += 1;
    if (ifMenuCtr >= ifMenuCnt) ifMenuCtr = 0; 
  
    lcd.cursor();
    switch(ifMenuCtr) {
      case 0: lcd.setCursor(0, 1); break; 
      case 1: lcd.setCursor(4, 1); break;
      default: lcd.noCursor(); break;
    }
  }
}

void mainMenu(uint8_t x, uint8_t rspeed) {

  if (ritmode) {
    RIT += ((x == DIR_CW) ? 1 : -1);
    if (RIT < -99) RIT = -99;
    if (RIT > 99) RIT = 99;
  }
  
  if (subMenuList[subMenuCtr] == "Freq"){
    u.vfo[u.currentBand][u.vfomode] += ((x == DIR_CW) ? (fStep*rspeed) : -(fStep*rspeed));
    if (u.vfo[u.currentBand][u.vfomode] < bandLimits[u.currentBand][0]) u.vfo[u.currentBand][u.vfomode] = bandLimits[u.currentBand][0];
    if (u.vfo[u.currentBand][u.vfomode] > bandLimits[u.currentBand][1]) u.vfo[u.currentBand][u.vfomode] = bandLimits[u.currentBand][1];
  }

  if (subMenuList[subMenuCtr] == "Mode"){
    u.mod += ((x == DIR_CW) ? 1 : -1);
    if (u.mod < 0) u.mod = 3;
    if (u.mod > 3) u.mod = 0;   
  }

  if (subMenuList[subMenuCtr] == "Band"){
    u.currentBand += ((x == DIR_CW) ? 1 : -1);
    if (u.currentBand >= bandCnt) u.currentBand = 0;
    if (u.currentBand < 0) u.currentBand = bandCnt-1;
    if (u.currentBand == DEFAULTBAND) {
      digitalWrite(BAND_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH); 
    } else {
      digitalWrite(BAND_PIN, LOW);
      digitalWrite(LED_PIN, LOW); 
    }    
  }

  if (subMenuList[subMenuCtr] == "Step"){   
    stepmode += ((x == DIR_CW) ? 1 : -1);
    
    u.stepmode = stepmode;
    if (stepmode < 0) stepmode = 3;
    if (stepmode > 3) stepmode = 0;
    fStep = stpModes[stepmode];
  }          

  if (subMenuList[subMenuCtr] == "VFO"){
    u.vfomode =! u.vfomode;
    frequency = u.vfo[u.currentBand][u.vfomode]; 
  }

  if (subMenuList[subMenuCtr] == "RIT"){   
    u.ritmode = ritmode = 1;
  } else {
    u.ritmode = ritmode = 0;          
  }
  
  setFrequency(u.vfo[u.currentBand][u.vfomode]);
}

void ifMenu(uint8_t x, uint8_t rspeed) {

  if (ifMenuList[ifMenuCtr] == "Band"){
    u.bfoBand += ((x == DIR_CW) ? 1 : -1);
    if (u.bfoBand >= bandCnt) u.bfoBand = 0;
    if (u.bfoBand < 0) u.bfoBand = bandCnt-1;
  }
  
  if (ifMenuList[ifMenuCtr] == "Freq"){
    // u.bfo[u.currentBand] = bfo[u.currentBand];
    u.bfo[u.bfoBand] += ((x == DIR_CW) ? 1*rspeed : -1*rspeed);
  }
  updateIFDisplay(false);
  
}

// Standard setup() and loop()
void setup(void)
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(BAND_PIN, OUTPUT);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.noCursor();

  setupNav();
  //EEPROM.put(0, u); // Uncomment this line for first upload, then comment out and upload again

  
  // Raduino setup.
  EEPROM.get(0, u);

  u.cal = CAL_VALUE;

  if (u.mod > CWU) {
    u.mod = LSB;
  }
  if (u.stepmode > 4) {
    u.stepmode = stepmode;
  }
  stepmode = u.stepmode;
  fStep = stpModes[stepmode];

  if (u.vfomode > 1) {
    u.vfomode = vfomode;
  }

  if (u.currentBand >= bandCnt || u.currentBand < 0) u.currentBand = 0;
  if (u.currentBand == DEFAULTBAND) {
    digitalWrite(BAND_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH); 
  } else {
    digitalWrite(BAND_PIN, LOW);
    digitalWrite(LED_PIN, LOW); 
  }

  if (u.vfo[u.currentBand][u.vfomode] > bandLimits[u.currentBand][1]) {
    frequency = u.vfo[u.currentBand][u.vfomode];
  } else {
    frequency = bandLimits[u.currentBand][0];
  }
 
  unsigned long warmUpFreq = 60000000UL;

  Si.init(25000000);
  Si.correction(1700);
  Si.disable(2);
  delay(500);
  
  // SIOUT_2mA, SIOUT_4mA, SIOUT_6mA and SIOUT_8mA
  Si.setPower(2, SIOUT_4mA);

  Si.setFreq(2, warmUpFreq);

  Si.reset();

  setFrequency(frequency);

  Serial.begin(9600);
}

void loop(void) {
  static bool prevMenuRun = true;
  static uint16_t count = 0;
  static uint32_t timeCount = 0;

  switch(swCtl.read())
  {
    case MD_KeySwitch::KS_NULL:       /* Serial.println("NULL"); */   break;
    case MD_KeySwitch::KS_PRESS:      chSubMenu(); break;
    case MD_KeySwitch::KS_LONGPRESS:  chMainMenu();   break;
    default:                          break;
  }

  uint8_t x = RE.read();
  uint8_t rspeed;

  #if ENABLE_SPEED
    rspeed = RE.speed();
    rspeed /= 2;
    if (rspeed < 1) rspeed = 1;
  #endif
  
  if (x) {
  
    if (topMenuList[topMenuCtr] == "Main") {    
      mainMenu(x, rspeed);
    }

    if (topMenuList[topMenuCtr] == "BFO") {    
      ifMenu(x, rspeed);
    }
    
    //return;
  } else if (millis() - timeCount >= 10000) {
    if (!memSaved) {
      u.stepmode = stepmode;
      EEPROM.put(0, u);
      memSaved = 1;
    }
    if (topMenuList[topMenuCtr] == "Main") {
      mainDisplay(false);
    }
    timeCount = millis();
  }
}

// Yes BFO is IF Freq. Minus 1.5 khz for USB. And Plus 1.5 Khz for LSB
