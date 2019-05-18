#include <ESP8266WiFi.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <Ticker.h>

#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
volatile int t=0;
volatile unsigned long int lastisr=0;
volatile unsigned char timerStateMachine=0;
volatile unsigned int heaterCounterValue=0;
ICACHE_RAM_ATTR void IntCallback()  {
  if(lastisr+5 < millis()){
    lastisr = millis();
    timerStateMachine=0;
    if(heaterCounterValue !=0)
    {
      timer1_write(heaterCounterValue);//max 50000--> ez megfelel 10ms
    }
  }
  if(lastisr>millis()+6)
  {
    lastisr = millis();
  }
}

ICACHE_RAM_ATTR void onTimerISR()  {
  if(timerStateMachine==0)
  {
    digitalWrite(14, 1);
    timerStateMachine++;
    timer1_write(1000);
    return;
  }
  if(timerStateMachine==1)
  {
    digitalWrite(14, 0);
  }
}
void callback(char* topic, byte* payload, unsigned int length) {

  int precent;
  if(!strcmp(topic,"/home/heatspeed"))
  {
   
    char* c= (char*)malloc(sizeof(char)*(length+1));
    for (int i = 0; i < length; i++) {
      c[i]=(char)payload[i];
    }
    c[length]='\0';
    sscanf(c,"%i",&precent);
    free(c);
   /* Serial.println("-----------------------------");
    Serial.println(topic);
    Serial.println(precent);
    Serial.println("-----------------------------");*/
    change_heating_speed(precent);
  }
}
void change_heating_speed(int precent)
{
  if(precent>100) precent=100;
  if(precent<0) precent=0;
  if(precent==0)
  {
    heaterCounterValue=0;
  }
  else
  {
    //heaterCounterValue=49000-(4800*precent)/100;
    heaterCounterValue=49000-(480*precent);
  
  }
  Serial.print("HS===");
  Serial.println(heaterCounterValue);
}



// DHT Sensor
const int DHTPin = 5;//D1
uint8_t GPIO_Pin = 2;//D2
// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);


const char* ssid = "Erikovszki";//WIFI azonosito
const char* password = "1werwerwer";//WIFI jelszo
const char* mqtt_server = "192.168.43.42"; //Raspbery IP cime



// Temporary variables
static char celsiusTemp[7];
static char fahrenheitTemp[7];
static char humidityTemp[7];




WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      client.subscribe("/home/heatspeed");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  
  pinMode(14, OUTPUT);
  attachInterrupt(4, IntCallback, RISING);//interrupt

  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
 


            // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
            float h = dht.readHumidity();
            // Read temperature as Celsius (the default)
            float t = dht.readTemperature();
            // Read temperature as Fahrenheit (isFahrenheit = true)
            float f = dht.readTemperature(true);
            // Check if any reads failed and exit early (to try again).
            if (isnan(h) || isnan(t) || isnan(f)) {
           /*   Serial.println("Failed to read from DHT sensor!");*/
              strcpy(celsiusTemp,"Failed");
              strcpy(fahrenheitTemp, "Failed");
              strcpy(humidityTemp, "Failed");         
            }
            else{
              // Computes temperature values in Celsius + Fahrenheit and Humidity
              float hic = dht.computeHeatIndex(t, h, false);       
              dtostrf(hic, 6, 2, celsiusTemp);             
              float hif = dht.computeHeatIndex(f, h);
              dtostrf(hif, 6, 2, fahrenheitTemp);         
              dtostrf(h, 6, 2, humidityTemp);
              // You can delete the following Serial.print's, it's just for debugging purposes
           /*   Serial.print("Humidity: ");
              Serial.print(h);
              Serial.print(" %\t Temperature: ");
              Serial.print(t);
              Serial.print(" *C ");
              Serial.print(f);
              Serial.print(" *F\t Heat index: ");
              Serial.print(hic);
              Serial.print(" *C ");
              Serial.print(hif);
              Serial.print(" *F");
              Serial.print("Humidity: ");
              Serial.print(h);
              Serial.print(" %\t Temperature: ");
              Serial.print(t);
              Serial.print(" *C ");
              Serial.print(f);
              Serial.print(" *F\t Heat index: ");
              Serial.print(hic);
              Serial.print(" *C ");
              Serial.print(hif);
              Serial.println(" *F");*/
            }
            
  
        
    
    if(isnan(t)){
      sprintf (msg, "NaN");
    }else{
      int homerseklet = (int)((t)*100);
      int nedv = (int)((h)*100);
      sprintf (msg, "%i.%i %i.%i", homerseklet/100, (homerseklet%100),nedv/100, (nedv%100));
      //sprintf (msg, "%i.%i", homerseklet/100, nedv/100);
    }
 /*   Serial.print("Publish message: ");
    Serial.println(msg);*/
    client.publish("/home/temperature", msg);
    for(int i =0; i< 500; i++){
      client.loop();
      delay(10);
    }
}
