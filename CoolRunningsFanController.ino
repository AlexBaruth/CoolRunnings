/*-----( Import needed libraries )-----*/
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define ONE_WIRE_BUS                    //New One-Wire dual bus implementation
OneWire oneWireA(11);
OneWire oneWireB(12);
DallasTemperature sensorsA(&oneWireA);
DallasTemperature sensorsB(&oneWireB);

LiquidCrystal_I2C lcd(0x3F, 16, 2);

/*-----( Declare Variables )-----*/
int fanpwr = 9;   //Fan Pin Must be PWM pin
int gled = 5;     //Green LED
int yled = 8;     //Yellow LED
int rled = 7;     //Red LED
unsigned long lastmillis_term = 0;
unsigned long lastmillis_fan = 0;
unsigned long previousMillis = 0;
volatile unsigned long NbTopsFan1;
volatile unsigned long NbTopsFan2;
int hallsensor1 = 2;                    //Arduino pins 2 and 3 must be used - interrupt 0
int hallsensor2 = 3;                    //interrupt 1
unsigned long totcalc1;
unsigned long totcalc2;
int rpmcalc1;
int rpmcalc2;
volatile unsigned long calcsec1;
volatile unsigned long calcsec2;
boolean automode;
char *fanmode;
boolean manled;
boolean lowled;
boolean autobled = true;
boolean autoupdate = false;
boolean contemp = true;
boolean variableFan;
boolean switchTemp;
float tempIn;
float tempOut;

int pwmLow;
int pwmMed;
int pwmHigh;

float lowTemp = 0.0; //temperature defaults
float medTemp = 0.0;
float highTemp = 0.0;

int lowSet = 0;        //eeprom location for pwm
int medSet = 1;
int highSet = 2;
int timeSet = 3;       //eeprom location for poll interval
int variableFanSet = 4;
int autoFanSet = 5;
int conTempSet = 6;
int switchTempSet = 7;
int lowTempAdd = 10;    //eeprom location for temp
int medTempAdd = 15;
int highTempAdd = 20;

char junk = ' ';
unsigned long looptime;
unsigned long looptimeSec;

int pwmFan = 0;
int pwmFanConst = 0;
float disparity;
float fanPercent;

typedef struct fan1
{
  //Defines the structure for multiple fans and their dividers
  char fantype1;
  unsigned int fandiv1;
}
fanspec1;

typedef struct fan2
{
  //Defines the structure for multiple fans and their dividers
  char fantype2;
  unsigned int fandiv2;
}

fanspec2;

fanspec1 fanspace1[3] = {{0, 1}, {1, 2}, {2, 8}};  char fan1 = 1;
fanspec2 fanspace2[3] = {{0, 1}, {1, 2}, {2, 8}};  char fan2 = 1;

void rpm1 ()
//This is the function that the interupt calls
{
  NbTopsFan1++;
}

void rpm2 ()
//This is the function that the interupt calls
{
  NbTopsFan2++;
}

//Function Definitions for non arduino IDE
void handleSerial();
void autocontrol();
void setpwm();
void setTemp();
void pollTime();
void readtemps();
void serialoutput();
void autoFan();
void variableFanPercent();

void setup() {
  // start serial port to show results
  Serial.begin(9600);
  lcd.begin();  // initialize the lcd for 16 chars 2 lines, turn on backlight

  // Initialize the Temperature measurement library

  Serial.println("CoolRunnings v2.3 (Board v3.1)");
  Serial.print("Initializing Temperature Control Library Version ");
  Serial.println(DALLASTEMPLIBVERSION);
  Serial.println("Type o to view control output");          //print these commands during setup
  Serial.println("Type ? to view command list");

  lcd.setCursor(1, 0); //Start at character 0 on line 0
  lcd.print("Cool Runnings");
  lcd.setCursor(0, 1); //Start at character 0 on line 1
  lcd.print("Fan Control v2.3");

  pinMode(hallsensor1 , INPUT);
  pinMode(hallsensor2 , INPUT);
  attachInterrupt(0, rpm1, FALLING);
  attachInterrupt(1, rpm2, FALLING);


  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  sensorsA.begin();
  sensorsB.begin();

  pwmLow = EEPROM.read(lowSet);
  pwmMed = EEPROM.read(medSet);
  pwmHigh = EEPROM.read(highSet);
  looptimeSec = EEPROM.read(timeSet);
  looptime = (looptimeSec * 1000);
  variableFan = EEPROM.read(variableFanSet);
  automode = EEPROM.read(autoFanSet);
  contemp = EEPROM.read(conTempSet);
  switchTemp = EEPROM.read(switchTempSet);

  lowTemp = EEPROM.get(lowTempAdd, lowTemp);
  medTemp = EEPROM.get(medTempAdd, medTemp);
  highTemp = EEPROM.get(highTempAdd, highTemp);

  //TCCR1B = (TCCR1B & 0b11111000) | 0x05;  //for 30Hz pwm on pins 9 and 10. RPM reading works best with current filter capacitor values. Slight audible fan noise.
  TCCR1B = (TCCR1B & 0b11111000) | 0x01;  //for 31KHz pwm on pins 9 and 10. effective pwm range is good, but unable to find filter capacitor values to elminate pwm noise. audible noise is gone.

}       //--(end setup )---

void loop() {


  readtemps();
  handleSerial();
  autocontrol();

  disparity = (tempIn - tempOut);

  if (millis() - lastmillis_fan >= 2000)         // Interval at which to run fan rpm code. If this changes, so does the divider for rpmcaclc1/2
  {
    lastmillis_fan = millis();                   // Update lasmillis
    // Command all devices on bus to read temperature

    totcalc1 = ((NbTopsFan1 ) / fanspace1[fan1].fandiv1);
    totcalc2 = ((NbTopsFan2 ) / fanspace2[fan2].fandiv2);
    unsigned long currentMillis = millis();

    previousMillis = currentMillis;  //Is this supposed to be here?

    rpmcalc1 = ((totcalc1 - calcsec1) * 60 / 2 ); //calculate rpm, divide by number of seconds which timer is run
    rpmcalc2 = ((totcalc2 - calcsec2) * 60 / 2); //count for a few seconds and divide for a smoother result

    if (previousMillis == currentMillis)    //store total rpm calculations at each interval
    {
      calcsec1 = totcalc1;
      calcsec2 = totcalc2;
    }

  }// End fan code here


  if (millis() - lastmillis_term >= looptime) {       // Interval at which to run code

    lastmillis_term = millis();                   // Update lasmillis
    // Command all devices on bus to read temperature

    sensorsA.requestTemperatures(); // Send the command to get temperature readings
    sensorsB.requestTemperatures(); //Request temperatues at interval to avoid minor fluctuation at temp threshold value triggering fan

    if (autoupdate == true) //print values at the interval the code is run if autoupdate is enabled
    {
      serialoutput();
    }
  }

  if (variableFan) {
    autoFan();
    fanPercent = ((pwmFanConst / 255.0) * 100); //add .0 after 255 to cast to float
    variableFanPercent();

    lcd.setCursor(0, 0);                        //Start at character 0 on line 0 and print out lcd information
    lcd.print("T1 ");
    lcd.print(tempIn, 1);                       //print temperature and show one decimal place
    lcd.print(" ");
    lcd.setCursor(8, 0);                        //Start at character 8 on line 0
    lcd.print("T2 ");
    lcd.print(tempOut, 1);
    lcd.print("  ");
    lcd.setCursor(0, 1);                        //Start at character 0 on line 1
    lcd.print("Pwr:");
    lcd.print(fanPercent, 0);
    lcd.print("%  D:");
    lcd.print(disparity, 1);
    lcd.print("   ");
  }
  else if (!variableFan) {
    lcd.setCursor(0, 0);                        //Start at character 0 on line 0 and print out lcd information
    lcd.print("T1 ");
    lcd.print(tempIn, 1);                       //print temperature and show one decimal place
    lcd.print(" ");
    lcd.setCursor(8, 0);                        //Start at character 8 on line 0
    lcd.print("T2 ");
    lcd.print(tempOut, 1);
    lcd.print("  ");
    lcd.setCursor(0, 1);                        //Start at character 0 on line 1
    lcd.print("Fan: ");
  }

  if (automode == true) {
    lcd.print("Auto ");
    lcd.print(fanmode);
    lcd.print("   ");
  }
  else if (!automode && !variableFan) {
    lcd.print("Manual ");
    lcd.print(fanmode);
    lcd.print("   ");
  }

  lowled = digitalRead(gled); // read state of fan low speed led

  //Backlight controls
  if (manled == false && autobled == false) //manual off
  {
    lcd.noBacklight();
  }
  if (manled == true && autobled == false) //manual on
  {
    lcd.backlight();
  }
  if (autobled == true && lowled == true)  // auto on
  {
    lcd.backlight();
  }
  if (autobled == true && lowled == false) //auto off
  {
    lcd.noBacklight();
  }

  if (fanmode == "Off")       //set leds to turn off or on with fan speeds
  {
    analogWrite(gled, 0);
    analogWrite(yled, 0);
    analogWrite(rled, 0);
  }
  else if (fanmode == "Low")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 0);
    analogWrite(rled, 0);
  }
  else if (fanmode == "Medium")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 255);
    analogWrite(rled, 0);
  }
  else if (fanmode == "High")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 255);
    analogWrite(rled, 255);
  }
}

void handleSerial() {

  while (Serial.available()) {
    switch (Serial.read()) {

      case 'a':
        automode = true;
        variableFan = false;
        EEPROM.write(variableFanSet, variableFan);
        EEPROM.write(autoFanSet, automode);
        break;

      case 'l':
        analogWrite(fanpwr, pwmLow);
        automode = false;
        fanmode = "Low";
        variableFan = false;
        break;

      case 'm':
        analogWrite(fanpwr, pwmMed);
        automode = false;
        fanmode = "Medium";
        variableFan = false;
        break;

      case 'h':
        analogWrite(fanpwr, pwmHigh);
        automode = false;
        fanmode = "High";
        variableFan = false;
        break;

      case 'k':
        analogWrite(fanpwr, 0);
        automode = false;
        fanmode = "Kill";
        variableFan = false;
        break;

      case 'b':
        lcd.backlight();
        manled = true;
        autobled = false;
        break;

      case 'd':
        lcd.noBacklight();
        manled = false;
        autobled = false;
        break;

      case 'u':           //enable automatic backlight
        autobled = true;
        break;

      case 'r':
        autoupdate = true;
        break;

      case 's':
        autoupdate = false;
        break;

      case 'f':
        contemp = true;
        EEPROM.write(conTempSet, contemp);
        break;

      case 'c':
        contemp = false;
        EEPROM.write(conTempSet, contemp);
        break;

      case 't':
        setTemp();
        break;

      case 'p':
        setpwm();
        break;

      case 'z':
        Serial.print(F("Feel the Rhythm! Feel the Rhyme! Get on up, it's bobsled time!"));
        break;

      case 'o':
        serialoutput();
        break;

      case 'i':
        pollTime();
        break;

      case 'w':
        if (switchTemp) {
          switchTemp = false;
        }
        else if (!switchTemp) {
          switchTemp = true;
        }
        EEPROM.write(switchTempSet, switchTemp);
        break;

      case 'q':
        lcd.begin();
        break;

      case 'v':
        variableFan = true;
        automode = false;
        EEPROM.write(variableFanSet, variableFan);
        EEPROM.write(autoFanSet, automode);
        break;

      case '?':
        Serial.println();
        Serial.println(F("Serial Commands"));
        Serial.println(F("o = Show Control Output"));
        Serial.println(F("a = Automode On"));
        Serial.println(F("v = Continuously Variable Fan Mode"));
        Serial.println(F("l = Manual Fan Low"));
        Serial.println(F("m = Manual Fan Medium"));
        Serial.println(F("h = Manual Fan High"));
        Serial.println(F("k = Manual Fan Kill"));
        Serial.println(F("b = LCD Backlight Bright"));
        Serial.println(F("d = LCD Backlight Dark"));
        Serial.println(F("u = Auto Backlight Mode"));
        Serial.println(F("q = Restart LCD display"));
        Serial.println(F("r = Auto Update Terminal(Recurring)"));
        Serial.println(F("s = Stop Auto Update Terminal"));
        Serial.println(F("f = Switch to Fahrenheit"));
        Serial.println(F("c = Switch to Celsius"));
        Serial.println(F("w = Switch T1 to T2"));
        Serial.println(F("e = Switch T2 to T1"));
        Serial.println(F("p = Set PWM values(Advanced)"));
        Serial.println(F("t = Set temperature thresholds(Advanced)"));
        Serial.println(F("i = Set timer interval(Advanced)"));
        Serial.println(F("? = Print Command List"));

        break;
    }
  }
}

void readtemps() { //temp sensor reading
  if (contemp  && switchTemp) {
    tempIn = (sensorsA.getTempFByIndex(0));
    tempOut = (sensorsB.getTempFByIndex(0));
  }
  if (!contemp && switchTemp) {
    tempIn = (sensorsA.getTempCByIndex(0));
    tempOut = (sensorsB.getTempCByIndex(0));
  }
  if (!contemp && !switchTemp) {
    tempOut = (sensorsA.getTempCByIndex(0));
    tempIn = (sensorsB.getTempCByIndex(0));
  }
  if (contemp && !switchTemp) {
    tempOut = (sensorsA.getTempFByIndex(0));
    tempIn = (sensorsB.getTempFByIndex(0));
  }
}


void autocontrol()            //temp based control
{
  if ((tempIn > tempOut + lowTemp) && (tempIn <= tempOut + medTemp) && automode && !variableFan)      //threshold temps for low auto speed here
  {
    analogWrite(fanpwr, pwmLow);                                           //low pwm setting here
    fanmode = "Low";
  }
  else if ((tempIn > tempOut + medTemp) && (tempIn <= tempOut + highTemp) && automode && !variableFan)   //threshold temps for medium auto speed here
  {
    analogWrite(fanpwr, pwmMed);                                           //medium pwm setting here
    fanmode = "Medium";
  }
  else if ((tempIn > tempOut + highTemp) && automode && !variableFan)                            //threshold temps for high auto speed here
  {
    analogWrite(fanpwr, pwmHigh);                                           //high pwm setting here maximum is 255(not actually pwm anymore)
    fanmode = "High";
  }
  else if (automode && !variableFan)                                                    //auto mode is true if manual mode is off
  {
    analogWrite(fanpwr, 0);                                             //fan is off if non of above criteria is met
    fanmode = "Off";
  }
}

void setpwm()
{
  Serial.println("Value from 0 to 255 for fan low power setting.(default 70)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    pwmLow = Serial.parseFloat();
    EEPROM.write(lowSet, pwmLow);
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(pwmLow, DEC);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
  }
  Serial.println("Value from 0 to 255 for fan Medium power setting. (default 120)");
  while (Serial.available() == 0) ;
  {
    pwmMed = Serial.parseFloat();
    EEPROM.write(medSet, pwmMed);
    Serial.print("Value set to: ");
    Serial.println(pwmMed, DEC);
    while (Serial.available() > 0)
    {
      junk = Serial.read() ;
    }
  }
  Serial.println("Value between 0 and 255 for fan High power setting. (default 255)");
  while (Serial.available() == 0) ;
  pwmHigh = Serial.parseFloat();
  EEPROM.write(highSet, pwmHigh);
  Serial.print("Value set to: ");
  Serial.println(pwmHigh, DEC);
  Serial.println("Done!");
  while (Serial.available() > 0)
  {
    junk = Serial.read() ;
  }

}


void setTemp()
{
  Serial.println("Degrees sensor 1 must differ from sensor 2 for low power setting.(default 1)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    lowTemp = Serial.parseFloat();
    EEPROM.put(lowTempAdd, lowTemp);
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(lowTemp, 2);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
  }
  Serial.println("Degrees sensor 1 must differ from sensor 2 for medium power setting. (default 4)");
  while (Serial.available() == 0) ;
  {
    medTemp = Serial.parseFloat();
    EEPROM.put(medTempAdd, medTemp);
    Serial.print("Value set to: ");
    Serial.println(medTemp, 2);
    while (Serial.available() > 0)
    {
      junk = Serial.read() ;
    }
  }
  Serial.println("Degrees sensor 1 must differ from sensor 2 for high power setting. (default 6)");
  while (Serial.available() == 0) ;
  highTemp = Serial.parseFloat();
  EEPROM.put(highTempAdd, highTemp);
  Serial.print("Value set to: ");
  Serial.println(highTemp, 2);
  Serial.println("Done!");
  while (Serial.available() > 0)
  {
    junk = Serial.read() ;
  }

}

void serialoutput()
{
  Serial.println();
  Serial.print("Control Information:   \r\n");
  Serial.print("TempInside:  ");
  Serial.print(tempIn);
  if (contemp)
  {
    Serial.print(" F");
  }
  else
  {
    Serial.print(" C");
  }
  Serial.println();
  Serial.print("TempOutside: ");
  Serial.print(tempOut);
  if (contemp)
  {
    Serial.print(" F");
  }
  else
  {
    Serial.print(" C");
  }
  Serial.println();
  Serial.print("Disparity: ");
  Serial.print(disparity, 1);

  Serial.println();
  if (rpmcalc1 != 0)
  {
    Serial.print (rpmcalc1, 1);
    Serial.print (" Fan1 rpm\r\n");
  }
  if (rpmcalc2 != 0)
  {
    Serial.print (rpmcalc2, 1);
    Serial.print (" Fan2 rpm\r\n");
  }

  if (!variableFan) {
    Serial.print ("Fan Power: ");
    Serial.println (fanmode);
  }
  else {
    Serial.print("Fan Power:");
    Serial.print(fanPercent, 0);
    Serial.println("%");
  }

  if (automode == true) {
    Serial.println(F("Mode: Auto (Incrementally Variable)"));
  }
  else if (variableFan == true) {
    Serial.println(F("Mode: Auto (Continuously Variable)"));
  }
  else {
    Serial.println(F("Mode: Manual"));
  }

  if (looptimeSec != 10) {
    Serial.print ("Custom Timer: ");
    Serial.print(looptimeSec);
    Serial.println(" sec");
  }

  if (pwmLow != 70 || pwmMed != 120 || pwmHigh != 255) {
    Serial.println("Custom PWM Enabled");
  }

  if (pwmLow != 70) {
    Serial.print("Low PWM Setting: ");
    Serial.println(pwmLow);
  }

  if (pwmMed != 120) {
    Serial.print("Medium PWM Setting: ");
    Serial.println(pwmMed);
  }

  if (pwmHigh != 255) {
    Serial.print("High PWM Setting: ");
    Serial.println(pwmHigh);
  }

  if (lowTemp != 1 || medTemp != 4 || highTemp != 6) {
    Serial.println("Custom Temp Enabled");
  }

  if (lowTemp != 1) {
    Serial.print("Low Temp Threshold: +");
    Serial.print(lowTemp);
    if (contemp)
    {
      Serial.println(" F");
    }
    else
    {
      Serial.println(" C");
    }
  }

  if (medTemp != 4) {
    Serial.print("Medium Temp Threshold: +");
    Serial.print(medTemp);

    if (contemp) {
      Serial.println(" F");
    }
    else {
      Serial.println(" C");
    }
  }

  if (highTemp != 6) {
    Serial.print("High Temp Threshold: +");
    Serial.print(highTemp);
    if (contemp)
    {
      Serial.println(" F");
    }
    else {
      Serial.println(" C");
    }
  }
  Serial.print("-------------------------------\r\n");
}

void pollTime() {

  Serial.println("Seconds the controller should check to see if it should do somthing in automode(default 10)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    looptimeSec = Serial.parseFloat();
    looptimeSec = constrain(looptimeSec, 1, 255);
    EEPROM.write(timeSet, looptimeSec);
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(looptimeSec, DEC);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
    looptime = (looptimeSec * 1000);
    Serial.println("Done!");
  }
}

void autoFan() {
  pwmFanConst = constrain(pwmFan, 0, 255);

  int offSet;
  int multiplier;

  multiplier = (255 / highTemp);                //how much the pwm should be affected by each degree changed
  offSet = (pwmLow - (lowTemp * multiplier));   //when lowTemp threshold is met, increase pwm to pwmLow value

  if (tempIn >= (tempOut + lowTemp)) {
    pwmFan = ((disparity * multiplier) + offSet);
  }
  else {
    pwmFan = 0;
  }
  analogWrite(fanpwr, pwmFanConst);
}


void variableFanPercent() {                             //Set fan mode which controls indicator LEDs

  if (fanPercent == 0) {                                //The fan won't spin until ~27% power(70PWM), move the scale up a bit to compensate.
    fanmode = "Off";
  }
  else if ((fanPercent <= 51) && (fanPercent > 0)) {
    fanmode = "Low";
  }
  else if ((fanPercent > 51) && (fanPercent <= 75)) {
    fanmode = "Medium";
  }
  else if (fanPercent > 75) {
    fanmode = "High";
  }
}
