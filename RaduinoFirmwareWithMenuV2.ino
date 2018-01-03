/*
 * Raduino firmware with MD_Menu
 * - Noli Rafallo
 *      4G1ZYS
 *      nrafallo@gmail.com
 *      
*/

#include <LiquidCrystal.h>
#include <MD_REncoder.h>
#include <MD_KeySwitch.h>
#include <MD_Menu.h>
#include <EEPROM.h>
#include <Wire.h>
#include <si5351mcu.h>
#include <CapacitiveSensor.h>

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

static LiquidCrystal lcd(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Rotary Encoder and KeySwitch Setup ------
extern MD_Menu M;

const uint8_t RE_A_PIN = 2;
const uint8_t RE_B_PIN = 3;
const uint8_t CTL_PIN = 4;

MD_REncoder  RE(RE_A_PIN, RE_B_PIN);
MD_KeySwitch swCtl(CTL_PIN);

// Menu Setup -----------------------------
const bool AUTO_START = true;
const uint32_t BAUD_RATE = 57600;
const uint16_t MENU_TIMEOUT = 5000;

const uint8_t LED_PIN = 13;

// Si5351 Setup ---------------------------

//Si5351 si5351;
Si5351mcu Si;

// Raduino Parameters ---------------------
// tuning range parameters
#define bfo_freq (11998000UL)

#define MIN_FREQ 7000000UL        // absolute minimum tuning frequency in Hz
#define MAX_FREQ 7800000UL        // absolute maximum tuning frequency in Hz

// USB/LSB parameters
#define CAL_VALUE 170000UL        // VFO calibration value
#define OFFSET_USB 1500           // USB offset in Hz [accepted range -10000Hz to 10000Hz]
#define VFO_DRIVE_LSB 4           // VFO drive level in LSB mode in mA [accepted values 2,4,6,8 mA]
#define VFO_DRIVE_USB 8           // VFO drive level in USB mode in mA [accepted values 2,4,6,8 mA]

// CW parameters
#define CW_SHIFT 500              // RX shift in CW mode in Hz, equal to sidetone pitch [accepted range 200-1200 Hz]
#define SEMI_QSK true             // whether we use semi-QSK (true) or manual PTT (false)
#define CW_TIMEOUT 350            // time delay in ms before radio goes back to receive [accepted range 10-1000 ms]

// RX-TX burst prevention
#define TX_DELAY 65               // delay (ms) to prevent spurious burst that is emitted when switching from RX to TX

// modes
#define LSB (0)
#define USB (1)
#define CWL (2)
#define CWU (3)

// Iambic capacitive keyer

#define ONEMINUTE 60000
#define ELEMPERWORD 50
#define WPM 15
#define DIT (ONEMINUTE / (ELEMPERWORD*WPM))
#define DAH DIT*3
#define DITPIN 5
#define LEDPIN A3 // key pin if you like
#define DAHPIN A0
#define SUPPIN A1
#define SPKPIN A2

int toggleD = 1;

CapacitiveSensor   ditSense = CapacitiveSensor(SUPPIN, DITPIN);
CapacitiveSensor   dahSense = CapacitiveSensor(SUPPIN, DAHPIN);


// User variables
unsigned long baseTune = 7100000UL;
int old_knob = 0;
int RXshift = 0;
unsigned long frequency;
unsigned long fStep = 100UL;
int fine = 0;
int memSaved = 1;

int8_t RIT = 0;

byte mode = LSB;
byte ritMode = 0;
byte stepMode = 0;

char c[17], b[10], printBuff[2][17];
bool locked = false;

struct userparameters {
  int USB_OFFSET = OFFSET_USB;
  unsigned long cal = CAL_VALUE;
  unsigned long frequency = frequency;
  byte mode = LSB;
  byte stepMode = 1;
  byte ritMode = 0;
  int8_t RIT = 0;
};

struct userparameters u;

// function prototypes for user nav and display callback
bool display(MD_Menu::userDisplayAction_t action, char *msg);

void setupNav(void)
{
  RE.begin();
  swCtl.begin();
  swCtl.enableRepeat(false);
}

// Function prototypes for variable get/set functions
void *mnuIValueRqst(MD_Menu::mnuId_t id, bool bGet);
void *mnuRadValueRqst(MD_Menu::mnuId_t id, bool bGet);

int8_t  int8Value = 0;

// Menu Headers ----------------------------
const PROGMEM MD_Menu::mnuHeader_t mnuHdr[] =
{
  { 10, "4G1ZYS",  10, 12, 0 },
  { 11, "Mode",   20, 20, 0 },
  { 12, "RIT", 21, 22, 0 },
  { 13, "Step", 23, 23, 0 },
};

// Menu Items ------------------------------
const PROGMEM MD_Menu::mnuItem_t mnuItm[] =
{
  // Starting (Root) menu
  { 10, "T/R Mode", MD_Menu::MNU_MENU, 11 },
  { 11, "RIT",     MD_Menu::MNU_MENU, 12 },
  { 12, "Step",        MD_Menu::MNU_MENU, 13 },

  // Input Data submenu
  { 20, "Set Mode", MD_Menu::MNU_INPUT, 10 },
  { 21, "Set RIT",    MD_Menu::MNU_INPUT, 11 },
  { 23, "Set Step",  MD_Menu::MNU_INPUT, 12 },
};

const char *trModes[] = { "LSB", "USB", "CWL", "CWU" };
const unsigned long *stpModes[] = { 100, 1000, 10000 };

// Input Items -----------------------------
const PROGMEM char listStep[] = "100H|1K|10K";
const PROGMEM char listRIT[] = "Off|On";
const PROGMEM char listModes[] = "LSB|USB|CWL|CWU";

const PROGMEM MD_Menu::mnuInput_t mnuInp[] =
{
  { 10, "Mode", MD_Menu::INP_LIST, mnuRadValueRqst, 3, 0, 0, 0, listModes },
  { 11, "RIT", MD_Menu::INP_LIST, mnuRadValueRqst, 3, 0, 0, 0, listRIT },
  { 12, "Step",    MD_Menu::INP_LIST,  mnuRadValueRqst,  3,   0,    0, 0, listStep },
};

MD_Menu M(navigation, display,
          mnuHdr, ARRAY_SIZE(mnuHdr),
          mnuItm, ARRAY_SIZE(mnuItm),
          mnuInp, ARRAY_SIZE(mnuInp));

// End of setup information ----------------
bool display(MD_Menu::userDisplayAction_t action, char *msg)
{
  static char szLine[LCD_COLS + 1] = { '\0' };

  switch (action)
  {
    case MD_Menu::DISP_CLEAR:
      lcd.clear();
      memset(szLine, ' ', LCD_COLS);
      break;

    case MD_Menu::DISP_L0:
      lcd.setCursor(0, 0);
      lcd.print(szLine);
      lcd.setCursor(0, 0);
      lcd.print(msg);
      break;

    case MD_Menu::DISP_L1:
      lcd.setCursor(0, 1);
      lcd.print(szLine);
      lcd.setCursor(0, 1);
      lcd.print(msg);
      break;
  }

  return (true);
}

MD_Menu::userNavAction_t navigation(uint16_t &incDelta)
{
  uint8_t re = RE.read();

  if (re != DIR_NONE)
  {
    incDelta = (M.isInEdit() ? (1 << abs(RE.speed() / 10)) : 1);
    return (re == DIR_CCW ? MD_Menu::NAV_DEC : MD_Menu::NAV_INC);
  }

  switch (swCtl.read())
  {
    case MD_KeySwitch::KS_PRESS:     return (MD_Menu::NAV_SEL);
    case MD_KeySwitch::KS_LONGPRESS: return (MD_Menu::NAV_ESC);
  }

  return (MD_Menu::NAV_NULL);
}

// Callback code for menu set/get input values
// Raduino menu

void *mnuRadValueRqst(MD_Menu::mnuId_t id, bool bGet)
{
  static byte mode_i = 0, ritmode = 0, stepmode = 0;

  switch (id)
  {
    case 10:
      if (bGet)
        return ((void *)&mode_i);
      else
      {
        mode = mode_i;
      }
      break;
    case 11:
      if (bGet)
        return ((void *)&ritmode);
      else
      {
        u. ritMode = ritMode = ritmode;
      }
      break;
    case 12:
      if (bGet)
        return ((void *)&stepmode);
      else
      {
        fStep = stpModes[stepmode];
        stepMode = stepmode;
      }
      break;
  }
  memSaved = 0;
  setFrequency(frequency);
  updateDisplay(true);
  return (nullptr);
}

void *mnuIValueRqst(MD_Menu::mnuId_t id, bool bGet)
{
  static int8_t rit = 0;
  switch (id)
  {
    case 13:
      if (bGet)
        return ((void *)&rit);
      else
      {
        u.RIT = RIT = rit;
      }
      break;
  }
  setFrequency(frequency);
  updateDisplay(true);
  return (nullptr);
}

void setFrequency(unsigned long f) {
  uint64_t osc_f;

  if (!ritMode)
    RIT = 0;

  switch (mode) {
    case LSB:
      Si.setFreq(2, bfo_freq - f + RIT);
      break;
    case USB:
      Si.setFreq(2, bfo_freq + f + RIT);
      break;
    case CWL:
      Si.setFreq(2, bfo_freq - f - CW_SHIFT + RIT);
      break;
    case CWU:
      Si.setFreq(2, bfo_freq + f  + CW_SHIFT + RIT);
      break;
    default:
      Si.setFreq(2, bfo_freq - f + RIT);
      mode = u.mode = LSB;
  }
  Si.enable(2);
  frequency = u.frequency = f;
  updateDisplay(true);
  memSaved = 0;
}

void updateDisplay(bool refresh) {
  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa((frequency + fine), b, DEC);
  strcpy(c, "");

  strcpy(c, "A  ");

  byte p = strlen(b);

  strncat(c, &b[0], p - 6);
  strcat(c, ",");
  strncat(c, &b[p - 6], 3);
  strcat(c, ".");
  strncat(c, &b[p - 3], 2);

  strcat(c, "  ");
  strcat(c, trModes[mode]);

  c[16] = '\0';
  printLine(0, c, refresh);

  // Print on line 2
  ultoa((fStep), b, DEC);
  strcpy(c, "");

  strcpy(c, "S ");

  p = strlen(b);

  if (fStep > 999) {
    strncat(c, &b[0], p - 3);
    strcat(c, ",");
  }
  strncat(c, &b[p - 3], 3);

  if (ritMode) {
    char rit[5];
    sprintf(rit, "%3i", RIT);
    for (byte i = strlen(c); i < 11; i++) {
      strcat(c, " ");
    }
    strcat(c, "R ");
    strcat(c, rit);
  }
  c[16] = '\0';
  printLine(1, c, refresh);
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

// Capacitive Keyer
void senseKey() {

  long dit = ditSense.capacitiveSensor(30);
  long dah = dahSense.capacitiveSensor(30);
  switch (toggleD) {
    case 1:
      if (dit > 50) {
        sound(DIT);
      }
      break;
    case 0:
      if (dah > 50) {
        digitalWrite(LEDPIN, HIGH);
        sound(DAH);
      }
      break;
  }
  toggleD ^= 1;
}

void sound(int eDelay) {
  analogWrite(SPKPIN, 127);
  delay(eDelay);
  analogWrite(SPKPIN, 0);
  digitalWrite(LEDPIN, LOW);
  delay(DIT);
}

// Standard setup() and loop()
void setup(void)
{
  pinMode(LED_PIN, OUTPUT);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.noCursor();

  setupNav();

  M.begin();
  M.setMenuWrap(true);
  M.setAutoStart(AUTO_START);
  M.setTimeout(MENU_TIMEOUT);

  // Raduino setup.
  EEPROM.get(0, u);

  u.cal = CAL_VALUE;

  if (u.mode > CWU) {
    u.mode = mode;
  }
  mode = u.mode;

  if (u.stepMode > 4) {
    u.stepMode = stepMode;
  }
  stepMode = u.stepMode;

  if (u.frequency > 7000000UL) {
    frequency = u.frequency;
  } else {
    frequency = baseTune;
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

  // capacitive keyer
  ditSense.set_CS_AutocaL_Millis(0xFFFFFFFF);
  dahSense.set_CS_AutocaL_Millis(0xFFFFFFFF);
}

void loop(void)
{
  static bool prevMenuRun = true;
  static uint16_t count = 0;
  static uint32_t timeCount = 0;

  if (prevMenuRun && !M.isInMenu())
    updateDisplay(true);
  prevMenuRun = M.isInMenu();
  
  if (!M.isInMenu() && !AUTO_START)
  {
    uint16_t dummy;

    if (navigation(dummy) == MD_Menu::NAV_SEL)
      M.runMenu(true);
  }

  if (!M.isInMenu())
  {
    uint8_t x = RE.read();
    if (x) {
    #if ENABLE_SPEED
    #endif

      if (ritMode) {
        RIT += ((x == DIR_CW) ? 1 : -1);
        if (RIT < -99) RIT = -99;
        if (RIT > 99) RIT = 99;
      } else {
        frequency += ((x == DIR_CW) ? fStep : -fStep);
        if (frequency < MIN_FREQ) frequency = MIN_FREQ;
        if (frequency > MAX_FREQ) frequency = MAX_FREQ;
      }
      setFrequency(frequency);
      return;
    } else if (millis() - timeCount >= 10000) {
      if (!memSaved) {
        u.frequency = frequency;
        u.mode = mode;
        u.stepMode = stepMode;
        EEPROM.put(0, u);
        memSaved = 1;
      }
      updateDisplay(false);
      timeCount = millis();
    }
    //senseKey();
  }
  M.runMenu();
}
