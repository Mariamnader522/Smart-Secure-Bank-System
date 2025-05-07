// Bank Security System with Professional Day/Night Web UI
#include <WiFi.h>
#include <DHT.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// —— Wi-Fi credentials —————————————————————————————
const char* ssid     = "menna";
const char* password = "menna185";
WiFiServer server(80);

// —— IR sensor pins & state ——————————————————————————
int serverIR   = 4, vaultIR = 19, employeeIR = 35, counterIR = 34;
String serverIRText, vaultIRText, employeeIRText, counterIRText;
int dayModeCheck = 0;  // 0 = day (vault only), 1 = night (all four)

// —— Environment sensors ——————————————————————————
const int firePin  = 23; // LOW = fire
const int smokePin = 18; // LOW = smoke
#define DHTPIN   5
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);
String fireText, smokeText, tempText, humText;

// —— Keypad & LCD & Buzzer —————————————————————————
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13,12,14,27}, colPins[COLS] = {26,25,33,32};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);
const int buzzerPin = 2;
const String correctCode = "1234";
String inputCode = "";
int wrongAttempts = 0, maxAttempts = 3;
unsigned long stateStart = 0;
const unsigned long buzzerDuration = 7000;  // 7s buzzer
const unsigned long lockDuration   =15000;  //15s lockout

enum State { WELCOME, ENTER, VAULT_OPEN, LOCKED_OUT } state;

// — Function Prototype —
void updateSensors();

void setup(){
  Serial.begin(115200);
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi IP=" + WiFi.localIP().toString());
  server.begin();

  pinMode(serverIR, INPUT_PULLUP);
  pinMode(vaultIR,  INPUT_PULLUP);
  pinMode(employeeIR, INPUT_PULLUP);
  pinMode(counterIR,  INPUT_PULLUP);
  pinMode(firePin,  INPUT_PULLUP);
  pinMode(smokePin, INPUT_PULLUP);
  dht.begin();

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.setCursor(4,0); lcd.print("Welcome");
  state = WELCOME;
}

void loop(){
  unsigned long now = millis();
  char key = keypad.getKey();

  // — Keypad state machine —
  switch(state){
    case WELCOME:
      if(key=='*'){
        inputCode="";
        lcd.clear(); lcd.setCursor(0,0); lcd.print("Enter pincode:");
        lcd.setCursor(6,1);
        state = ENTER;
      }
      break;

    case ENTER:
      if(key>='0' && key<='9' && inputCode.length()<4){
        inputCode += key;
        lcd.setCursor(6 + inputCode.length()-1,1);
        lcd.print('*');
      } else if(key=='#'){
        lcd.clear();
        if(inputCode==correctCode){
          lcd.setCursor(1,0); lcd.print("Successful ");
          lcd.setCursor(1,1); lcd.print("Authentication");
          delay(2000);
          lcd.clear(); lcd.setCursor(1,0); lcd.print("Vault Unlocked");
          state = VAULT_OPEN;
          wrongAttempts = 0;
        } else {
          wrongAttempts++;
          if(wrongAttempts<maxAttempts){
            lcd.setCursor(0,0); lcd.print("Incorrect Pin");
            delay(1500);
            lcd.clear(); lcd.setCursor(0,0); lcd.print("Enter Pincode:");
            lcd.setCursor(6,1);
          } else {
            stateStart = now;
            lcd.setCursor(0,0); lcd.print("Failed authentication");
            delay(1500);
            lcd.clear();
            lcd.setCursor(0,0); lcd.print("Locked Out");
            lcd.setCursor(0,1); lcd.print("Try in 10s");
            digitalWrite(buzzerPin, HIGH);
            state = LOCKED_OUT;
          }
        }
        inputCode="";
      }
      break;

    case VAULT_OPEN:
      if(key=='A'){
        lcd.clear(); lcd.setCursor(2,0); lcd.print("Vault Locked");
        delay(1500);
        lcd.clear(); lcd.setCursor(4,0); lcd.print("Welcome");
        state = WELCOME;
      }
      break;

    case LOCKED_OUT:
      if(now - stateStart >= buzzerDuration) digitalWrite(buzzerPin, LOW);
      if(now - stateStart >= lockDuration){
        wrongAttempts = 0;
        lcd.clear(); lcd.setCursor(4,0); lcd.print("Welcome");
        state = WELCOME;
      }
      break;
  }

  // — Web server —
  WiFiClient client = server.available();
  if(!client) return;

  String header, line;
  unsigned long start = now;
  while(client.connected() && millis()-start < 2000){
    if(!client.available()) continue;
    char c = client.read();
    header += c;
    if(c=='\n'){
      if(line.length()==0){
        // Mode toggle
        if(header.indexOf("GET /daymode")>=0)   dayModeCheck=0;
        if(header.indexOf("GET /nightmode")>=0) dayModeCheck=1;
        updateSensors();

        // Response headers
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close\n");

        // Build HTML with 1-second auto-refresh
        String bg       = (dayModeCheck==0)? "#f5f5f5" : "#1a1a1a";
        String fg       = (dayModeCheck==0)? "#333"   : "#eee";
        String cardBg   = (dayModeCheck==0)? "#fff"   : "#333";
        String border   = (dayModeCheck==0)? "#ccc"   : "#444";
        String vaultStatus = (state==VAULT_OPEN)? "Unlocked" : "Locked";

        client.println(
          "<!DOCTYPE html><html lang=\"en\"><head>"
            "<meta http-equiv=\"refresh\" content=\"1\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            "<title>Bank Security</title>"
            "<style>"
              ":root { --bg:" + bg + "; --fg:" + fg + "; --card:" + cardBg + "; --border:" + border + "; }"
              "body{margin:0;font-family:Arial,sans-serif;background:var(--bg);color:var(--fg);}"
              "header{padding:1rem;text-align:center;background:var(--card);border-bottom:1px solid var(--border);}"
              "h1{margin:0;font-size:1.8rem;}"
              ".mode-switch{margin-top:0.5rem;}"
              "button{padding:0.6rem 1.2rem;margin:0 .3rem;border:none;border-radius:4px;font-size:1rem;cursor:pointer;}"
              "button.active{background:#4CAF50;color:#fff;}"
              "button.inactive{background:var(--border);color:var(--fg);}"
              ".container{display:flex;flex-wrap:wrap;justify-content:center;gap:1rem;padding:1rem;}"
              ".card{background:var(--card);border:1px solid var(--border);border-radius:8px;padding:1rem;min-width:200px;box-shadow:2px 2px 6px rgba(0,0,0,0.1);}"
              ".card h2{margin-top:0;font-size:1.2rem;}"
              ".card p{margin:0.5rem 0;font-size:1rem;}"
              "table{width:100%;border-collapse:collapse;margin-top:0.5rem;}"
              "th,td{border:1px solid var(--border);padding:0.5rem;text-align:center;}"
              "th{background:#4CAF50;color:#fff;}"
            "</style>"
          "</head><body>"
        );

        // Header with mode buttons
        client.println(
          "<header><h1>Bank Security Dashboard</h1>"
          "<div class=\"mode-switch\">"
            "<a href=\"/daymode\"><button class=\"" + String((dayModeCheck==0)?"active":"inactive") + "\">Day Mode</button></a>"
            "<a href=\"/nightmode\"><button class=\"" + String((dayModeCheck==1)?"active":"inactive") + "\">Night Mode</button></a>"
          "</div></header>"
        );

        // Main cards
        client.println(
          "<div class=\"container\">"
            "<div class=\"card\"><h2>Vault Status</h2><p><strong>" + vaultStatus + "</strong></p></div>"
            "<div class=\"card\"><h2>IR Sensors</h2>"
              "<table><tr><th>Area</th><th>Status</th></tr>"
              "<tr><td>Server</td><td>"   + serverIRText   + "</td></tr>"
              "<tr><td>Vault</td><td>"    + vaultIRText    + "</td></tr>"
              "<tr><td>Employee</td><td>" + employeeIRText + "</td></tr>"
              "<tr><td>Counter</td><td>"  + counterIRText  + "</td></tr>"
              "</table></div>"
            "<div class=\"card\"><h2>Environment</h2>"
              "<p>Fire: <strong>"    + fireText  + "</strong></p>"
              "<p>Smoke: <strong>"   + smokeText + "</strong></p>"
              "<p>Temp: <strong>"    + tempText  + "</strong></p>"
              "<p>Humidity: <strong>" + humText   + "</strong></p>"
            "</div>"
          "</div>"
        );

        // Close HTML
        client.println("</body></html>");
        break;
      }
      line = "";
    } else if(c!='\r'){
      line += c;
    }
  }
  client.stop();
}

void updateSensors(){
  vaultIRText  = (digitalRead(vaultIR)==HIGH)    ? "Secure"       : "Breach";
  if(dayModeCheck==1){
    serverIRText   = (digitalRead(serverIR)==HIGH)   ? "Secure"       : "Breach";
    employeeIRText = (digitalRead(employeeIR)==HIGH) ? "Secure"       : "Breach";
    counterIRText  = (digitalRead(counterIR)==HIGH)  ? "Secure"       : "Breach";
  } else {
    serverIRText = employeeIRText = counterIRText = "Deactivated";
  }
  fireText  = (digitalRead(firePin)==LOW)  ? "Fire Detected" : "No Fire";
  smokeText = (digitalRead(smokePin)==LOW) ? "Smoke Detected": "No Smoke";
  float h = dht.readHumidity(), t = dht.readTemperature();
  tempText = isnan(t) ? "Error" : String(t,1) + " °C";
  humText  = isnan(h) ? "Error" : String(h,1) + " %";
}
