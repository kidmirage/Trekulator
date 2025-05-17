/* 
 *  Trekulator Calculator Code
 *        
 */
#include "LedController.hpp"
#include "printHelpers.h"
#include <TJpg_Decoder.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPIFFS.h"
#include <TFT_eSPI.h> // Hardware-specific library

// Used to setup a once per second interrupt.
hw_timer_t *My_timer = NULL;
bool advanceLED = false;
int ledShowing = 0;

// Number of items in an array.
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// Invoke custom board library.
TFT_eSPI tft = TFT_eSPI(); 

// Calibrate touch screen.
#define CALIBRATION_FILE "/touch_calibration"
        
// TFT board device chip select pins.
#define DISPLAY_CS    15
#define TOUCH_CS      27
#define SDCARD_CS     5

// TFT board SPI connections.
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18
 
// I2S board connections.
#define I2S_DOUT      22
#define I2S_BCLK      26
#define I2S_LRC       25

// TFT display Connections.
#define DIN 13
#define CS 12
#define CLK 14

// Number of digits in 7-seg display.
#define NUM_SEGS 8

// Keypad Connections.
#define R1 16
#define R2 17
#define R3 21
#define R4 33
#define C1 32
#define C2 35
#define C3 34
#define C4 39
#define C5 36

#define NUM_ROWS 4
#define NUM_COLS 5
int rows[] = {R1, R2, R3, R4};
int cols[] = {C1, C2, C3, C4, C5};

// Create a 7-Segment display controller object.
LedController<1,1> lc;

// Create an Audio object.
Audio audio;

// Map the keys on the keypad.
char keymap[NUM_ROWS][NUM_COLS] = {
  {'7', '8', '9', 'c', 'C',}, 
  {'4', '5', '6', 'S', 'P',}, 
  {'1', '2', '3', '/', 'x',},
  {'0', '.', '=', '+', '-'}
};

// Calculator variables.
char nextKey;           // The next key from the keypad.
String first = "";      // Remember the first op.
double total = 0;       // The current running total.
bool isPlaySound = false; // Controls the playback of sounds.

// Keep a list of all the sound files for each chracter.
#define MAX_FILES 50
String kirk_sounds[MAX_FILES];
int kirk_number = 0;
String spock_sounds[MAX_FILES];
int spock_number = 0;
String uhura_sounds[MAX_FILES];
int uhura_number = 0;
String images[MAX_FILES];
int image_number = 0;

// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);
  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

// Play the sound passed over I2S.
void playSound() {
  
  // Play the sound passed.
  audio.setVolume(18);
  isPlaySound = true;
  drawLEDs(TFT_RED);
  audio.connecttoFS(SD, "/Trekulator_Sound.mp3");
}

// Check the pad for a key press.
char getKey() {
  char key = 0;
  for (int i = 0; i < NUM_ROWS; i++) {
    digitalWrite(rows[i], HIGH);
    for (int j = 0; j < NUM_COLS; j++) {
      if(digitalRead(cols[j])==HIGH){
        // Debounce.
        delay(1);
        if(digitalRead(cols[j])==HIGH) { 
          key = keymap[i][j];
          while(digitalRead(cols[j])==HIGH) {
            // Wait for key up.
            delay(1);
          }
        }
      }
    }
    digitalWrite(rows[i], LOW);
  }
  return key;
}

// Diddle the 7-segment display (cylon style).
void showWaiting() {
  lc.clearMatrix();     //  Clear the display.
  for (int i = 0; i < NUM_SEGS; i++) {
    lc.setChar(0, i, ' ', true);
    delay(50);
    lc.setChar(0, i, ' ', false);
  }
  for (int i = 0; i < NUM_SEGS; i++) {
    lc.setChar(0, NUM_SEGS-i-1, ' ', true);
    delay(50);
    lc.setChar(0, NUM_SEGS-i-1, ' ', false);
  }  
  for (int i = 0; i < NUM_SEGS; i++) {
    lc.setChar(0, i, ' ', true);
    delay(50);
    lc.setChar(0, i, ' ', false);
  }
  showDisplay(first);
}

// Wait for a key press. 
char waitKey() {
  while( 1 ) {
   
    nextKey = getKey();
    if (nextKey == 0) {
      checkForTouch();
      moveLED();
      continue;
    }
    return nextKey;
  }
}

void moveLED() {
 if (advanceLED) {
    drawLED(ledShowing, TFT_BLACK);
    ledShowing = (ledShowing + 1) % 5;
    drawLED(ledShowing, TFT_YELLOW);
    advanceLED = false;
  }
}

// Called when a sound file is finished playing.
void audio_eof_mp3(const char *info){
  isPlaySound = false;
  drawLEDs(TFT_BLACK);
  showDisplay(first);
}

// Set all five fake LEDs to a specific color.
void drawLEDs(int color) {
  tft.fillRect(100, 0, 300, 30, color);
}

// Set a single fake LED to a specific color.
void drawLED(int led, int color) {
  switch (led) {
    case 0:
      tft.fillCircle(130, 20, 10, color);
      break;
    case 1:
      tft.fillCircle(194, 20, 10, color);
      break;
    case 2:
      tft.fillCircle(260, 20, 10, color);
      break;
    case 3:
      tft.fillCircle(325, 20, 10, color);
      break;
    case 4:
      tft.fillCircle(392, 20, 10, color);
      break;
  }
}

// Drive the playback of sound files.
void audioTask(void *parameter) {
  int i = 0;
  bool blink = true;
  while(true){
    i = i + 1;
    if (isPlaySound) {
      audio.loop();
    } else {
      sleep(1);
    }
  }
}

// Setup the audio playback handler.
void audioInit() {
  xTaskCreatePinnedToCore(
    audioTask,             // Function to implement the task.
    "audioplay",           // Name of the task.
    5000,                  // Stack size in words.
    NULL,                  // Task input parameter.
    2 | portPRIVILEGE_BIT, // Priority of the task.
    NULL,                  // Task handle.
    1                      // Core where the task should run.
  );
}


// Convert the number passed to a String. Maximize the display digits.
String convertToString(double total) {
  char result[30];
  String value;
  int decimals = NUM_SEGS + 2;
  
  // See if result that is too big can be shortened. Get the value with
  //  extra decimal places.
  dtostrf (total, NUM_SEGS, decimals, result);
  value = String(result);
  value.trim();

  // Remove trailing zeros from the value if there is a decimal place.
  bool hasdecimal = false;
  if (value.indexOf('.') != -1) {
    hasdecimal = true;
    value = removeTrailingZeros(value);
  } 

  // Check for still too big.
  if (hasdecimal) {
    if (value.length()-1 > NUM_SEGS) {
      // Still too big. See if decimal places can be reduced.
      int decimalpos = value.indexOf('.');
      if (decimalpos > 0) {
        // At least one whole number so reduce decimals.
        int decimals = NUM_SEGS - decimalpos;
        if (decimals >= 2) {
          dtostrf (total, NUM_SEGS, decimals, result);
          String test = String(result);
          test = removeTrailingZeros(test);
          if (test.charAt(test.length()-1) != '.') {
            // There are still numbers in the fraction so OK.
            value = String(result);
            value.trim();
          } else {
            // Just a whole number so remove the decimal.
            test.remove(test.length()-1);
            value = test;
          }
        }
      } 
    }
  }
  return value;
}

// Remove the trailing zeros from the string passed.
String removeTrailingZeros(String value) {
  bool skipZero = true;
  for (int i = value.length()-1; i >= 0; i--) {
    char digit = value.charAt(i);
    if (digit == '0'  && skipZero) {
      value.remove(i);
      continue;
    } else {
      break;
    }
  }
  return value;
}

// Show the number passed on the 7-Segment display.
void showDisplay(String show) {
  // Clear the display.
  lc.clearMatrix();

  // If empty display "0.";
  if (show.length() == 0) {
    show = show + "0.";
  }

  // Add a decimal place at the end if there isn't one already.
  if (show.indexOf('.') == -1) {
    show = show + '.';
  }

  // Check for value too big.
  if (show.length()-1 > NUM_SEGS) {
    // Show as exponential.
    show = sci(show.toDouble(), 2);
  }
  int pos = 0;
  char digit;
  pos = 0;
  int len = show.length();

  bool decimal = false;
  for (int i = 0; i < len; i++) {
    digit = show[len-1-i];
    if (digit == '.') {
      decimal = true;
      continue;
    } else {
      lc.setChar(0, pos, digit, decimal);
      if (decimal) {
        decimal = false; // Already displayed.
      }
    }
    pos += 1;
  }
}

void printDirectory(File dir, int numTabs) {
    while(true) {
      File entry = dir.openNextFile();
  
      if (!entry) {
        dir.rewindDirectory();
        break;
    }
    
    for (uint8_t i=0; i<numTabs; i++)   Serial.print('\t');
    Serial.print(entry.name());
    
    if (entry.isDirectory()) {
 
      Serial.println("/");
      printDirectory(entry, numTabs+1);
    } 

    else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
  }
}

// Fetch all of the sound files names in a particular folder.
int getSoundFileNames(File root, String folderName, String fileNameList[]) {
  int nSounds = 0;
  File entry;
  File dir;
  
  // Find the sound folder.
  while (true) {
    entry = root.openNextFile();
    if (!entry) {
      return 0;
    }
    if (entry.isDirectory() && (String(entry.name()) == "sounds")) {
      root.rewindDirectory();
      dir = entry;
      break;
    } else {
      entry.close();
    }
  }

  // Find the correct sound folder.
  while (true) {
    entry = dir.openNextFile();
    if (!entry) {
      return 0;
    }
    if (entry.isDirectory() && (String(entry.name()) == folderName)) {
      dir.rewindDirectory();
      dir = entry;
      break;
    } else {
      entry.close();
    }
  }

  // Count the number of files.
  while (true) {
    entry = dir.openNextFile();
    if (!entry) {
      dir.rewindDirectory();
      break;
    } else {
      nSounds = nSounds + 1;
    }
    entry.close();
  }

  // Add the file to the list.
  if (nSounds > MAX_FILES) {
    nSounds = MAX_FILES;
  }
  for(int i = 0; i < nSounds; i++){
    entry = dir.openNextFile();
    fileNameList[i] = strdup(entry.name());
    entry.close();
  }
  dir.rewindDirectory();
  
  // Return the number of sound files found.
  return nSounds;
}

// Fetch all of the image files names in a particular folder.
int getImageFileNames(File root, String fileNameList[]) {
  int nSounds = 0;
  File entry;
  File dir;
  
  // Find the sound folder.
  while (true) {
    entry = root.openNextFile();
    if (!entry) {
      return 0;
    }
    if (entry.isDirectory() && (String(entry.name()) == "images")) {
      root.rewindDirectory();
      dir = entry;
      break;
    } else {
      entry.close();
    }
  }

  // Count the number of files.
  while (true) {
    entry = dir.openNextFile();
    if (!entry) {
      dir.rewindDirectory();
      break;
    } else {
      nSounds = nSounds + 1;
    }
    entry.close();
  }

  // Add the file to the list.
  if (nSounds > MAX_FILES) {
    nSounds = MAX_FILES;
  }
  for(int i = 0; i < nSounds; i++){
    entry = dir.openNextFile();
    fileNameList[i] = strdup(entry.name());
    entry.close();
  }
  dir.rewindDirectory();
  
  // Return the number of sound files found.
  return nSounds;
}

// Respond to screen touches.
void checkForTouch() {
  uint16_t x, y;
  if (!isPlaySound) {
    digitalWrite(DISPLAY_CS, HIGH);
    digitalWrite(TOUCH_CS, LOW);
    digitalWrite(SDCARD_CS, HIGH);
    if (tft.getTouch(&x, &y)) {
      if (x < 40) {
        if (y > 45 && y < 105) {
          // Top button.
          String soundPath = "/Photon_Torpedo_Sound.mp3";
          audio.setVolume(20);
          isPlaySound = true;
          audio.connecttoFS(SD, soundPath.c_str());
        } else if (y > 140 && y < 200) {
          // Middle button.
          showImages();
        } else if (y > 220 && y < 280) {
          // Bottom button.
          String soundPath = "/Scanner_Sound.mp3";
          audio.setVolume(20);
          isPlaySound = true;
          audio.connecttoFS(SD, soundPath.c_str());
        }
      } else if (y > 50 && y < 250) {
        if (x >140 && x < 240) {
          // Spock.
          String soundPath = "/sounds/spock/" + spock_sounds[random(spock_number)];
          audio.setVolume(20);
          isPlaySound = true;
          audio.connecttoFS(SD, soundPath.c_str());
        } else if (x > 240 && x < 380) {
          // Kirk.
          String soundPath = "/sounds/kirk/" + kirk_sounds[random(kirk_number)];
          audio.setVolume(20);
          isPlaySound = true;
          audio.connecttoFS(SD, soundPath.c_str());
        } else if (x > 380 && x < 470) {
          // Uhura.
          String soundPath = "/sounds/uhura/" + uhura_sounds[random(uhura_number)];
          audio.setVolume(20);
          isPlaySound = true;
          audio.connecttoFS(SD, soundPath.c_str());
        }
      }
    }
  }
}

// Get a number from the user.
String getInput(char init) {
  String result = "";
  uint16_t x, y;
  
  if (init > 0) {
    result = result + init;
    showDisplay(result);
  } 
  while( 1 ) {
    checkForTouch();
    moveLED();
    nextKey = getKey();
    if (nextKey == 0) {
      // No key.
      delay(10);
      continue;
    }
    if((nextKey >= '0' && nextKey <= '9') ||            // Digit.
       (nextKey == '.' && result.indexOf('.') == -1) ||   // First decimal.
       (nextKey == '-' && result.length() == 0)) {        // Beginning subtraction.
      if (result.length() >= NUM_SEGS) {
        // When buffer full don't add any more digits.
        continue;
      }
      if (nextKey == '.' && result.length() == 0) {
        // First character is a . so have to add an anchor character of zero.
        result = result + '0';
      } else if (nextKey == '0' && result.length() == 0) {
        // Don't add leading zeros.
        continue;
      }
      result = result + nextKey;
      showDisplay(result);
    } else if (nextKey == 'c') {
      // Only clear the current value.
      result = "";
      showDisplay(result);
    } else if (nextKey == 'C') {
      // Clear the current value and the total.
      result = "";
      total = 0;
      first = "";
      showDisplay(result);
    } else {
      return result;
    }
  }
}

// Timer ISR.
void IRAM_ATTR onTimer(){
  advanceLED = true;
}

void showImages() {
  for (int i = 0; i < image_number; i++) {
    TJpgDec.drawSdJpg(39, 34, "/images/" + images[i]);
    for (int j = 0; j < 3; j++) {
      moveLED();
      delay(3000);
    }
  }
  TJpgDec.drawFsJpg(39, 34, "/Background_Image.jpg");
}

// Initialize the Trekulator code.
void setup() {
  // Used to calibrate the touch screen.
  uint16_t calibrationData[5];
  uint8_t calDataOK = 0;
  
  // Start serial connection.
  Serial.begin(115200);
  Serial.println("Starting...");

  // Start the SPIFFS file system.
  SPIFFS.begin();

  // Disable all TFT board devices.
  pinMode(DISPLAY_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SDCARD_CS, HIGH);

  // Initialize the SPI bus.
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // Start the TFT display.
  tft.init();

  // Start microSD card controller.
  if(!SD.begin(SDCARD_CS)) {
    Serial.println("Error accessing microSD card!");
    //while(true); 
  } else {
    File root = SD.open("/");
    // printDirectory(root, 2);
    // Load up the file sound file names.
    kirk_number = getSoundFileNames(root, "kirk", kirk_sounds); 
    spock_number = getSoundFileNames(root, "spock", spock_sounds); 
    uhura_number = getSoundFileNames(root, "uhura", uhura_sounds);

    // Load up the file image file names.
    image_number = getImageFileNames(root, images); 
  }
  
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);  
  tft.setTextSize(2);
  
  digitalWrite(DISPLAY_CS, HIGH); // Done with display for now.

  // Check if calibration file exists.
  // SPIFFS.remove(CALIBRATION_FILE);
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f) {
      if (f.readBytes((char *)calibrationData, 14) == 14)
        calDataOK = 1;
      f.close();
    }
  }
  
  if (calDataOK) {
    // Calibration data valid.
    tft.setTouch(calibrationData);
    Serial.println("Touch calibrated.");
  } else {
    // data not valid. recalibrate
    tft.calibrateTouch(calibrationData, TFT_WHITE, TFT_BLACK, 15);
    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calibrationData, 14);
      f.close();
    }
  }

  // Setup the keypad matrix.
  pinMode(R1, OUTPUT); 
  pinMode(R2, OUTPUT); 
  pinMode(R3, OUTPUT); 
  pinMode(R4, OUTPUT);
  digitalWrite(R1, LOW);
  digitalWrite(R2, LOW);
  digitalWrite(R3, LOW);
  digitalWrite(R4, LOW);
  pinMode(C1, INPUT);
  pinMode(C2, INPUT);
  pinMode(C3, INPUT);
  pinMode(C4, INPUT);
  pinMode(C5, INPUT);

  // Setup the I2S sound board.
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  // Setup the playback handler.
  audioInit();
  
  // Set sound volume.
  audio.setVolume(21);

  // Only one speaker.
  audio.forceMono(true);

  // Play the Trekulator sound.
  playSound(); 

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  // Draw the image into the screen window.
  TJpgDec.drawSdJpg(39, 34, "/Background_Image.jpg");

  // Setup the 1 second timer interrupt.
  My_timer = timerBegin(1000000);
  timerAttachInterrupt(My_timer, &onTimer);
  timerAlarm(My_timer, 1000000, true, 0);
  
  // Here a new LedController object is created without hardware SPI.
  // NOTE: This probably could have used the TFT SPI connections saving 3 pins.
  lc=LedController<1,1>(DIN,CLK,CS);
  lc.setIntensity(15);   // Set the brightness to a high values
  showDisplay("");       //  and clear the display.
}

// Main loop.
void loop() {
  
  // Get a starting input number from the user.
  String input = getInput(0);
  first = input;
  total = input.toDouble();

  // Keep processing operations till the user clears.
  while (true) {
    // nextKey will have the key that caused the input routine to exit.
    switch (nextKey) {
      case '=':
        lc.clearMatrix();
        // Play the annoying sound.
        playSound();
        waitKey();
        break;
      case 'c':
        waitKey();
        break;
      case 'C': 
        // Reset.
        total = 0;
        showDisplay("");
        first = "";
        return;

      case '+':
        input = getInput(0);
        total = total + input.toDouble();
        first = convertToString(total);
        showDisplay(first);
        break;

      // Subtract.
      case '-':
        input = getInput(0);
        total = total - input.toDouble();
        first = convertToString(total);
        showDisplay(first);
        break;

      // Multiply.
      case 'x':
        input = getInput(0);
        total = total * input.toDouble();
        first = convertToString(total);
        showDisplay(first);
        break;

      // Divide.
      case '/':
        input = getInput(0);
        total = total / input.toDouble();
        first = convertToString(total);  
        showDisplay(first); 
        break;

      // Square root.
      case 'S':
        total = sqrt(total);
        first = convertToString(total);
        showWaiting();  
        // showDisplay(first); 
        waitKey();
        break;

      // Percentage.
      case 'P':
        total = total / 100;
        first = convertToString(total);  
        showDisplay(first); 
        waitKey();
        break;

      // Default. Break out of switch.
      default:
        if (nextKey == 'c' || nextKey == '=') {
          nextKey = 0;  // Ignore
        }
        input = getInput(nextKey);
        total = input.toDouble();
        break;
    }
  }
}
