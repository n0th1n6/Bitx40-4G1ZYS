
/*
 * EasyBitx Dual Band
 * - Noli Rafallo
 *      4G1ZYS
 *      nrafallo@gmail.com
 *      
 *  This vfo/bfo uses the following libraries
 *  
 *  From MajicDesigns - https://github.com/MajicDesigns?tab=repositories
 *    MD_Rencoder
 *    MD_KeySwitch
 *    
 *  EtherKit Si5351 Library from - https://github.com/etherkit/Si5351Arduino
 *  
 *  All of the libraries used are available also from library manager
*/

#include <LiquidCrystal.h>
#include <MD_REncoder.h>
#include <MD_KeySwitch.h>
#include <EEPROM.h>
#include <Wire.h>
#include <si5351.h>

#define EBITX // define this either as RADUINO or EBITX to seleect the board
/* LCD type */
#define  LCD_ROWS  2
#define  LCD_COLS  16


#ifdef RADUINO
  // LCD pin definitions 
  // Raduino on Bitx40
  #define  LCD_RS   8
  #define  LCD_ENA  9
  #define  LCD_D4   10
  #define  LCD_D5   LCD_D4+1
  #define  LCD_D6   LCD_D4+2
  #define  LCD_D7   LCD_D4+3

  // Rotary Encoder and KeySwitch Setup ------
  #define RE_A_PIN  2
  #define RE_B_PIN  3
  #define CTL_PIN   4

#endif

#ifdef EBITX
  /* LCD pin-out for easybitx */
  #define  LCD_RS   5
  #define  LCD_ENA  6
  #define  LCD_D4   7
  #define  LCD_D5   LCD_D4+1
  #define  LCD_D6   LCD_D4+2
  #define  LCD_D7   LCD_D4+3

  // Rotary Encoder and KeySwitch Setup ------
  #define RE_A_PIN  3
  #define RE_B_PIN  2
  #define CTL_PIN   11

#endif

#define ENABLE_SPEED 1 // if sant to enable velocity tuning wherein the faster the knob rotation is the faster the frequency changes
#define LED_PIN     13

// tuning range parameters
// USB/LSB parameters
#define CAL_VALUE     (196100ULL)   // VFO calibration value
#define IF_OFFSET     1500UL        // USB offset in Hz [accepted range -10000Hz to 10000Hz]
#define VFO_DRIVE_LSB 4             // VFO drive level in LSB mod in mA [accepted values 2,4,6,8 mA]
#define VFO_DRIVE_USB 8             // VFO drive level in USB mod in mA [accepted values 2,4,6,8 mA]
#define FREQ_MULT     100ULL

// CW parameters
#define CW_SHIFT 500              // RX shift in CW mode in Hz, equal to sidetone pitch [accepted range 200-1200 Hz]

// modes
#define LSB (0)
#define USB (1)
#define CWL (2)
#define CWU (3)

// available PINS 5,6,7,22,A0, A1, A3
#define BAND_PIN    (5)
#define DEFAULTBAND 0 //this is base on the order you define in the variables below

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
const char *ifMenuList[] = { "BFO", "IF"};
const char *trModes[] = { "LSB", "USB", "CWL", "CWU" };
const char *stpStr[] = { " 10", "100", " 1K", "10K" };
const unsigned int stpModes[] = { 10, 100, 1000, 10000 };
const unsigned long bandLimits[2][2] = {{7000000UL, 7800000UL}, {14000000UL, 14600000UL}};
unsigned long long bfo = 9997469ULL;
unsigned long long if_freq = 9997000ULL;
const unsigned long bfoLimits[2] = {9990000UL, 10010000UL};

int topMenuCtr = 0;
int subMenuCtr = 0;
int ifMenuCtr = 0;

struct userparameters {
  int USB_OFFSET;
  unsigned long cal = CAL_VALUE;
  int mod = LSB;
  int stepmode = 1;
  int ritmode = 0;
  int vfomode = 0;
  int8_t RIT = 0;
  unsigned long bfo = 9997469UL;
  unsigned long vfo[2][2] = {{7000000UL, 7800000UL}, {14000000UL, 14600000UL}};;
  int currentBand = 0;
  unsigned long ifFreq = 9997469ULL;
  bool isSaved = false;
};

struct userparameters u;

static LiquidCrystal lcd(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

MD_REncoder  RE(RE_A_PIN, RE_B_PIN);
MD_KeySwitch swCtl(CTL_PIN);

Si5351 si5351;

void setupNav(void)
{
  RE.begin();
  swCtl.begin();
  swCtl.enableRepeat(false);
}


void setFrequency(unsigned long f) {

  unsigned long long vfo_freq = 0ULL;
  if (!ritmode)
    RIT = 0;

  

  switch (u.mod) {
    case LSB:
      vfo_freq = (u.ifFreq - (f + RIT)) * FREQ_MULT;
      break;
    case USB:
      vfo_freq = ((f + RIT) - u.ifFreq) * FREQ_MULT;
      break;
    case CWL:
      vfo_freq = (u.ifFreq - (f + RIT - CW_SHIFT)) * FREQ_MULT;
      break;
    case CWU:
      vfo_freq = ((f + RIT + CW_SHIFT) - u.ifFreq) * FREQ_MULT;
      break;
    default:
      break;
  }
  si5351.set_freq(vfo_freq, SI5351_CLK0);
  si5351.output_enable(SI5351_CLK0, 1);


  if (topMenuList[topMenuCtr] == "Main") {
    mainDisplay(true);
  }

  if (topMenuList[topMenuCtr] == "BFO") {
    updateIFDisplay(false);
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

  strcpy(c, "BFO");

  ultoa(u.bfo, b, DEC);

  byte p = strlen(b);

  for (byte i = strlen(c); i < 4; i++) {
    strcat(c, " ");
  }

  strncat(c, &b[0], p - 6);
  strcat(c, ",");
  strncat(c, &b[p - 6], 3);
  strcat(c, ".");
  strncat(c, &b[p - 3], 3);

  c[16] = '\0';
  printLine(0, c, refresh);

  strcpy(c, "IF");

  ultoa(u.ifFreq, b, DEC);

  p = strlen(b);

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

    switch (ifMenuCtr) {
      case 0: lcd.setCursor(4, 0); break;
      case 1: lcd.setCursor(4, 1); break;
      default: break;
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

  if (ifMenuList[ifMenuCtr] == "BFO") {
    u.bfo += ((x == DIR_CW) ? 1*rspeed : -1*rspeed);
    if (u.bfo > bfoLimits[1]) u.bfo = bfoLimits[0];
    if (u.bfo < bfoLimits[0]) u.bfo = bfoLimits[1];
    bfo = (u.mod == LSB) || ((u.mod - 2) == LSB) ? (u.bfo - IF_OFFSET) : (u.bfo + IF_OFFSET);
    //bfo = u.bfo;
  
    bfo = bfo * FREQ_MULT;
    si5351.set_freq(bfo, SI5351_CLK2);
    si5351.output_enable(SI5351_CLK2, 1);
  }

  if (ifMenuList[ifMenuCtr] == "IF") {
    u.ifFreq += ((x == DIR_CW) ? 1*rspeed : -1*rspeed);
    if (u.ifFreq > bfoLimits[1]) u.ifFreq = bfoLimits[0];
    if (u.ifFreq < bfoLimits[0]) u.ifFreq = bfoLimits[1];
  }

  setFrequency(u.vfo[u.currentBand][u.vfomode]); 
}

// Standard setup() and loop()
void setup(void)
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(BAND_PIN, OUTPUT);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.noCursor();
  Serial.begin(9600);


  setupNav();
  
  // check saved values
  userparameters utemp = u;
  EEPROM.get(0, u);

  if (!u.isSaved) {
    utemp.isSaved = true;
    EEPROM.put(0, utemp);
    delay(500);
    EEPROM.get(0, u);
  }

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
 
  unsigned long long warmUpFreq = 60000000ULL;

  bool i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if(!i2c_found)
  {
    Serial.println("No Si5351 found!");
  }

  delay(500);

  warmUpFreq = warmUpFreq * FREQ_MULT;
  si5351.set_freq(warmUpFreq, SI5351_CLK0);
  si5351.set_freq(warmUpFreq, SI5351_CLK2);
 
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_4MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_4MA);

  si5351.reset();
  setFrequency(frequency);
  bfo = (u.mod == LSB) || ((u.mod - 2) == LSB) ? (u.bfo - IF_OFFSET) : (u.bfo + IF_OFFSET);
  si5351.set_freq(bfo * FREQ_MULT, SI5351_CLK2);
  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK2, 1);

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
