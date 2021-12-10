#include <string> 
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include "ThingSpeak.h"


// Enter Telegram Bot token here
#define token "..."

// Wifi network credentials
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."


/* Value to GPIO Pin Mapping
 * 0 = 32
 * 1 = 33
 * 2 = 27
 * 3 = 14
 * 4 = 12
 * 5 = 13
 * 6 = 15
 * 7 = 4
 * 8 = 16
 * 9 = 17
 */

// ATM Class
class MONEY{

  public:
    // Stores the denominations available
    int notes[3] = {2000,1000,500};

    // Stores the total number of available notes
    int quant[3] = {5,10,10};
//    TaskHandle_t * tele;

    MONEY(){
    }

    // returns the balance from the EEPROM
    int getEEPROMBal(){
      return 100*EEPROM.read(0) + EEPROM.read(1)  ;
    }

    // returns to balance based on the available notes
    int getBalance(){
      int bal = 0;

      // iterates through all the notes and adds them
      for(int i = 0; i < 3; i++){
        bal += quant[i]*notes[i];
      }
  
      return bal;
    }

    // finds to number of each denomination to be withdrawn
    // Priority given to lesser number of notes
    void withdraw(int amount){
        int tot = 0;
        int temp = amount;
        int notes_withdrawn[3] = {0};
    
        for(int i = 0; i < 3 && amount != tot; i++){

          // finds the number of notes required for a particular denomination
          int numNotes = int(temp/notes[i]);
    
          if(numNotes <= quant[i]){
            tot += numNotes*notes[i];
            notes_withdrawn[i] = numNotes;
          }
          else{
            tot += quant[i]*notes[i];
            notes_withdrawn[i] = quant[i]; 
          }
    
          temp -= tot;
          
        }

        // to check if the available balance is sufficient
        // or if the number of required denomination is available
        if(amount == tot){
          for(int i = 0; i < 3; i++){
            quant[i] -= notes_withdrawn[i];  
          }
          Serial.println("Transaction of " + String(amount) + " was successfull. Thank You for choosing ESP32 Bank of IoT"); 
          for(int j = 0; j < 3; j++){
            Serial.print(String(notes_withdrawn[j]) + " notes of " + String(notes[j]) + " ; ");  
          }  
          Serial.print(" Withdrawn\n");
        }
        else{
          Serial.println("Withdrawal Amount of " + String(amount) + " is not available. Sorry for the inconvenience.");
        }

        updateBalance();
      }

    // to update the balance to the EEPROM
    void updateBalance(){
      int bal = getBalance();
      EEPROM.write(0,byte(int(bal/100)));
      EEPROM.write(1,byte(int(bal%100)));
  
      EEPROM.commit();
          
    } 
  
};

// WiFi Client class
WiFiClientSecure client;

// Class to interface with telegram bot (BotFather)
UniversalTelegramBot bot(token, client);

// Variable to keep track of time
unsigned long bot_lasttime; 
const unsigned long bot_time = 1000;

// LED Pin
const int ledPin = 2;

// Stores the value of OTP generated
int otp = 0;

// Stores the value of OTP entered by user
int otp_entered = 0;

// Enter ThingSpeak credentials here
unsigned long int ts_channel = 00;
const char * myWriteAPIKey = "...";

// Declaration of atm class
// Initiallized with a balance of 25000 INR
MONEY atm;

// Variable used to startup the otp verification on when it is generated through 
// telegram bot by "/login"
bool init_func = false;


// Task handle for verification of OTP, withdrawal of money
TaskHandle_t mainCode;

// Task handle for generation of OPT
TaskHandle_t TeleBotHandle;

// Function to read the sent messages to the Telegram Chat bot
// /login: generated a 2 digit One-Time-Password
// /balance: sends the current balance
void newMessage(int numMsg){
  for (int i = 0; i < numMsg; i++){
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if(text == "/login"){

      // generates a 2 digit otp
      otp = random(10,100);

      // prints the otp
      //Serial.println("Your OTP is: " + String(otp));

      // now verification can take place
      init_func = true;

      // sends the otp
      bot.sendMessage(chat_id,"Your OTP is: " + String(otp), "");
    }

    if(text == "/balance"){
      bot.sendMessage(chat_id,"Your account balance is: " + String(atm.getBalance()));
    }
  }

  // Uploads data to ThingSpeak
  // Sends current balance
  ThingSpeak.setField(1, atm.getBalance());

  // Sends nymber of 2000 notes available
  ThingSpeak.setField(2, atm.notes[0]);

  // Sends nymber of 1000 notes available
  ThingSpeak.setField(3, atm.notes[1]);

  // Sends nymber of 500 notes available
  ThingSpeak.setField(3, atm.notes[2]);

  // Writes to all the fields
  ThingSpeak.writeFields(1549752, myWriteAPIKey);

  // Wait for the data to be sent
  delay(20000);
}

// Function to init balance to 25000
void init_balance(int bal = 25000){
  // Writes the First 3 digits to 0 address
  EEPROM.write(0,byte(int(bal/100)));

  // Writes the last 2 digits to 1 address
  EEPROM.write(1,byte(int(bal%100)));

  // Saves the changes made
  EEPROM.commit();
}

// returns the pin tounched
int checkPin(){
  long int start = millis();
  long int now = millis();
  int arr[8] = {32,33,27,14,12,13,15,4};
  int i = 0;
  int num = -1;

  do{
    bool found = false;
    for(i = 0; i < 8 && !found; i++){

      // Checks for the touch pins
      if(touchRead(arr[i]) < 13){
//        Serial.println("Touch Pin " + String(arr[i]) + " Touch Val: " +String(touchRead(arr[i])) );
        num = i;
        found = true;
        delay(5);
      }
      else{
        num = -1;  
      }  
    }

    // Checks for the non touch pins
    // We have to give output high for the following to be selected
    if(digitalRead(16) == 1){
      //Serial.println("Pin 16 ON");
      num = 8;
    }
    if(digitalRead(17) == 1){
      //Serial.println("Pin 17 ON");
      num = 9;
    }
  }while(millis() - start < 50);

  //Serial.println("Selected: " + String(num));

  // returns the number associated with the pin
  return num;
}


// function to return the number types on the 
// keypad

// We have option to select the length in terms of the number of digits to be entered.
int getNum(int val){
  
  int count = 0;

  // counts the number of digits
  int counter2 = 0;

  // gets the initial value of the key pressed
  int num = checkPin();
  String enteredVal = "";
  
  //checks for the digit
  bool digit = true;
  
  // 1 iteration = 1 digit
  while(counter2 < val){
    digit = true;
    count = 0;
    while(digit){
      num = int(checkPin());
      
    // checks if the pin is touched for a long time or not
    if(num == checkPin() && num != -1 && count >= 3){
      // LED blinks when the number is selected for a feedback     
      delay(50);
      digitalWrite(ledPin,HIGH);
      delay(50);
      digitalWrite(ledPin,LOW);

      enteredVal = enteredVal + String(num);
      
      digit = false;
    }
    else if(num == int(checkPin()) && num != -1){
      count++;
      delay(20);
      // Serial.println("Count  = " + String(count));
    }

    }

    // waits for us to leave the pin 
    // checkPin() returns -1 when no pin is touched
    while(checkPin()!=-1 && count > 0){
        delay(100);
        // Serial.println("num = " + String(num) + "checkPin() = " + String(checkPin()));
    }

    // counts the number of digits
    counter2++;
  }

  return enteredVal.substring(enteredVal.length() - val).toInt();
}


// Function to connect to WiFi
void connectToWiFi(){
  Serial.print("Connecting to WiFi. ");
  Serial.print(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED){ 
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected"); 
  
}


// Function to configure the pins on ESP32
void configPins(){
  
  pinMode(ledPin, OUTPUT);
  
  digitalWrite(ledPin, LOW);

}


// This task handle runs on Core 0 
// Main objective is to verify the otp and enable the user to withdraw the amount
// also checks if the amount is available to withdraw
void main_code(void * para){

  // initializes the balance to 25000
  init_balance(25000);
  while(true){

    // only true when otp is send on telegram
    if(init_func){

      // allows the user to enter the otp
      otp_entered = getNum(2);
      if(otp == otp_entered){
        delay(200);

        // allows the user to enter the amount to withdraw
        Serial.println("Enter Amount to be Withdrawn using the Touch Pins");
        int getAmount = getNum(5);

        // withdraw the amount
        atm.withdraw(getAmount); 

        // stops the function from running until next time the otp is requested
        init_func = false;

        // Allows the user to check the balance
        Serial.println("Press 111 to Check balance or 999 to exit");

        // A watchdog timer s triggered when the prompt is not responded to 
        long int start = millis();
        int last = getNum(3);
        while((last != 111 or last != 999) || (millis() - start > 30000)){
          
          digitalWrite(ledPin,HIGH);
          if(last == 111){
            Serial.println("Your Balance is: " + String(atm.getBalance()));
            break;
          }
          else if(last == 999){
            Serial.println("Thank You for Choosing ESP32 Bank of IoT!");
            break;
          }  
          else{
            last = getNum(3);
          }
        }

        
        digitalWrite(ledPin,LOW);

        if(millis() - start > 30000){
          Serial.println("session Expired. Thank You for Choosing Us!");  
        }
      }
    }
    delay(500);
  }
}

void checkTeleMessage(void * para){
  while(true){
    if (millis() - bot_lasttime > bot_time){
      int numMsg = bot.getUpdates(bot.last_message_received + 1);
  
      while(numMsg){
        newMessage(numMsg);
        numMsg = bot.getUpdates(bot.last_message_received + 1);
      }
  
      bot_lasttime = millis();
    }
    

  }
}



void setup(){
  Serial.begin(115200);
  Serial.println();
  
  

  configPins();
  connectToWiFi();

  ThingSpeak.begin(client);
  
  
  
  xTaskCreatePinnedToCore(
      main_code, 
      "main_code",
      10000, 
      NULL, 
      1,  
      &mainCode,  
      0);
  delay(1000);
  xTaskCreatePinnedToCore(
      checkTeleMessage, 
      "checkTeleMessage",
      10000, 
      NULL, 
      1,  
      &TeleBotHandle,  
      1);

}

void loop(){
}
