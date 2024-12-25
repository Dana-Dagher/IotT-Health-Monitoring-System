#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SoftwareSerial.h>


#define RX 2
#define TX 3

String AP = "Dagher";       // AP NAME
String PASS = "70093284"; // AP PASSWORD
String API = "KGECUZSUJK6BXZQY";   // Write API KEY
String HOST = "api.thingspeak.com";
String PORT = "80";

SoftwareSerial esp8266(RX, TX);
LiquidCrystal_I2C lcd(0x27, 16, 2);

float pulse = 0;
float temp = 0;



int pulsePin = A0; // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 7; // pin to blink led at each beat
int fadePin = 13; // pin to do fancy classy fading blink at each beat
int fadeRate = 0; // used to fade LED on with PWM on fadePin

volatile int BPM; // int that holds raw Analog in 0. updated every 2mS
volatile int Signal; // holds the incoming raw data
volatile int IBI = 600; // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false; // "True" when User's live heartbeat is detected. "False" when nota "live beat".
volatile boolean QS = false; // becomes true when Arduoino finds a beat.

volatile unsigned long sampleCounter = 0; // used to determine pulse timing
volatile unsigned long lastBeatTime = 0; // used to find IBI
volatile int P = 512; // used to find peak in pulse wave, seeded
volatile int T = 512; // used to find trough in pulse wave, seeded
volatile int thresh = 525; // used to find instant moment of heart beat, seeded
volatile int amp = 100; // used to hold amplitude of pulse waveform, seeded
volatile boolean firstBeat = true; // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = false; // used to seed rate array so we startup with reasonable BPM
volatile int rate[10]; // array to hold last ten IBI values
int countTrueCommand = 0;
int countTimeCommand = 0;
bool found = false;

////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);        //start serial communication at 9600 baud rate
  lcd.begin(16, 2);         //initialize the LCD with 16 columns and 2 rows
  lcd.backlight();           // turn on the LCD backlight
  pinMode(blinkPin, OUTPUT); // pin that will blink to your heartbeat!
  pinMode(fadePin, OUTPUT); // pin that will fade to your heartbeat!

  interruptSetup();         // sets up to read Pulse Sensor signal every 2mS!
  esp8266.begin(115200);    // start software serial for ESP8266 at 115200 baud rate
  sendCommand("AT",5,"OK");  // check if the ESP8266 is responding
  sendCommand("AT+CWMODE=1",5,"OK");  // set ESP8266 to Wi-Fi client mode
  sendCommand("AT+CWJAP=\""+ AP +"\",\""+ PASS +"\"",20,"OK"); // connect to Wi-Fi
  delay(1000);

  esp8266.println("AT");   // send AT command to check ESP8266
  delay(1000);
  esp8266.println("AT+GMR");   // get firmware version
  delay(1000);
  esp8266.println("AT+CWMODE=3");// set ESP8266 to both client and access point mode
  delay(1000);
  esp8266.println("AT+RST");   // reset ESP8266
  delay(5000);
  esp8266.println("AT+CIPMUX=1");    // enable multiple connections
  delay(1000);

  String cmd = "AT+CWJAP=\"" + AP + "\",\"" + PASS + "\"";
  esp8266.println(cmd);            // enable multiple connections
  delay(1000);
  esp8266.println("AT+CIFSR");    // get IP address
  delay(1000);
}

/////////////////////////////////////////////////////////////////////////////////

void loop() {
  int field1 = BPM; // Get BPM value
  float field2 = read_temp(); // Get temperature value
  String getData = "GET /update?api_key="+ API +"&field1="+ String(field1) +"&field2="+ String(field2);
  
  sendCommand("AT+CIPMUX=1",5,"OK");    // enable multiple connections
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");  // start TCP connection
  sendCommand("AT+CIPSEND=0," +String(getData.length()+4),4,">");       // prepare to send data
  esp8266.println(getData);   // send data to ThingSpeak
  delay(1500);
  countTrueCommand++;
  sendCommand("AT+CIPCLOSE=0",5,"OK");  // close TCP connection 
  serialOutput();      // send data to serial monitor
  if (QS == true) { // A Heartbeat Was Found
    fadeRate = 255; // Makes the LED Fade Effect Happen, Set 'fadeRate' Variable to 255 to fade LED with pulse
    serialOutputWhenBeatHappens(); // A Beat Happened, Output that to serial.
    QS = false; // reset the Quantified Self flag for next time
  }
  ledFadeToBeat(); // Makes the LED Fade Effect Happen
  delay(20); // take a break
}

/////////////////////////////////////////////////////////////////////////////////////////

void sendCommand(String command, int maxTime, char readReplay[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  while(countTimeCommand < (maxTime*1))
  {
    esp8266.println(command);    //send command to ESP8266
    if(esp8266.find(readReplay))  //check for expected response
    {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true)
  {
    Serial.println("success");  // indicate success
    countTrueCommand++;
    countTimeCommand = 0;
  }
  
  if(found == false)
  {
    Serial.println("Fail");   // indicate fail
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  
  found = false;
 }

////////////////////////////////////////////
void ledFadeToBeat() {
  fadeRate -= 15; // set LED fade value
  fadeRate = constrain(fadeRate, 0, 255); // keep LED fade value from going into negative numbers!
  analogWrite(fadePin, fadeRate); // fade LED
}

////////////////////////////////////////////
void interruptSetup() {
  // Initializes Timer2 to throw an interrupt every 2mS.
  TCCR2A = 0x02; // DISABLE PWM ON DIGITAL PINS 3 AND 11, AND GO INTO CTC MODE
  TCCR2B = 0x06; // DON'T FORCE COMPARE, 256 PRESCALER
  OCR2A = 0X7C; // SET THE TOP OF THE COUNT TO 124 FOR 500Hz SAMPLE RATE
  TIMSK2 = 0x02; // ENABLE INTERRUPT ON MATCH BETWEEN TIMER2 AND OCR2A
  sei(); // MAKE SURE GLOBAL INTERRUPTS ARE ENABLED
}

////////////////////////////////////////////
void serialOutput() {
  arduinoSerialMonitorVisual('-', Signal); // goes to function that makes Serial Monitor Visualizer
}

//////////////////////////////////////////
void serialOutputWhenBeatHappens() {
  sendDataToSerial('B', BPM); // send heart rate with a 'B' prefix
  sendDataToSerial('Q', IBI); // send time between beats with a 'Q' prefix
}

///////////////////////////////////////////
void arduinoSerialMonitorVisual(char symbol, int data ) {
  const int sensorMin = 0; // sensor minimum, discovered through experiment
  const int sensorMax = 1024; // sensor maximum, discovered through experiment
  int sensorReading = data; // map the sensor range to a range of 12 options:
  int range = map(sensorReading, sensorMin, sensorMax, 0, 11);
  // do something different depending on the
  // range value:
  switch (range) {
    case 0:
      Serial.println(""); /////ASCII Art Madness
      break;
    case 1:
      Serial.println("---");
      break;
    case 2:
      Serial.println("------");
      break;
    case 3:
      Serial.println("---------");
      break;
    case 4:
      Serial.println("------------");
      break;
    case 5:
      Serial.println("--------------|-");
      break;
    case 6:
      Serial.println("--------------|---");
      break;
    case 7:
      Serial.println("--------------|-------");
      break;
    case 8:
      Serial.println("--------------|----------");
      break;
    case 9:
      Serial.println("--------------|----------------");
      break;
    case 10:
      Serial.println("--------------|-------------------");
      break;
    case 11:
      Serial.println("--------------|-----------------------");
      break;
  }
}

///////////////////////////////////////////////
void sendDataToSerial(char symbol, int data ) {
  Serial.print(symbol);
  Serial.println(data);
}

//////////////////////////////////////////////
ISR(TIMER2_COMPA_vect) { //triggered when Timer2 counts to 124
  cli(); // disable interrupts while we do this
  Signal = analogRead(pulsePin); // read the Pulse Sensor
  sampleCounter += 2; // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime; // monitor the time since the last beat to avoid noise
  if (Signal < thresh && N > (IBI / 5) * 3) {
    if (Signal < T) {
      T = Signal;
    }
  }
  if (Signal > thresh && Signal > P) {
    P = Signal;
  }
  if (N > 250) {
    if ((Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3)) {
      Pulse = true;
      digitalWrite(blinkPin, HIGH);
      IBI = sampleCounter - lastBeatTime;
      lastBeatTime = sampleCounter;
      if (secondBeat) {
        secondBeat = false;
        for (int i = 0; i <= 9; i++) {
          rate[i] = IBI;
        }
      }
      if (firstBeat) {
        firstBeat = false;
        secondBeat = true;
        sei();
        return;
      }
      word runningTotal = 0;
      for (int i = 0; i <= 8; i++) {
        rate[i] = rate[i + 1];
        runningTotal += rate[i];
      }
      rate[9] = IBI;
      runningTotal += rate[9];
      runningTotal /= 10;
      BPM = 60000 / runningTotal;
      QS = true;
      pulse = BPM;
    }
  }
  if (Signal < thresh && Pulse == true) {
    digitalWrite(blinkPin, LOW);
    Pulse = false;
    amp = P - T;
    thresh = amp / 2 + T;
    P = thresh;
    T = thresh;
  }
  if (N > 2500) {
    thresh = 512;
    P = 512;
    T = 512;
    lastBeatTime = sampleCounter;
    firstBeat = true;
    secondBeat = false;
  }
  sei();
}

//////////////////////////////////////////////
float read_temp() {
  int temp_val = analogRead(A1);
  float voltage = temp_val * (8.0 / 1024.0);
  float temp= voltage*10;
  //float mv = (temp_val/1024.0)*5000;
  //float cel = mv/10;
  //temp = (cel*9)/5 + 32;
  Serial.print("Temperature:");
  Serial.println(temp);
  Serial.print("heartbeat:");
  Serial.println(BPM);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BPM :");
  lcd.setCursor(7,0);
  lcd.print(BPM);
  lcd.setCursor(0,1);
  lcd.print("Temp.:");
  lcd.setCursor(7,1);
  lcd.print(temp);
  lcd.setCursor(13,1);
  lcd.print("C");
  return temp;
}
