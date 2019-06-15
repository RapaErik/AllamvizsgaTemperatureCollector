#include <ESP8266WiFi.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <Ticker.h>

#define DHTTYPE DHT11 // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

//volatile int t = 0;
volatile unsigned long int lastisr = 0;
volatile unsigned char timerStateMachine = 0;
volatile unsigned int heaterCounterValue = 0;

// DHT Sensor
const int DHTPin = 5; //D1
uint8_t GPIO_Pin = 2; //D2
// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);
// Temporary variables
static char celsiusTemp[7];
static char fahrenheitTemp[7];
static char humidityTemp[7];

const char *ssid = "Erikovszki";           //WIFI azonosito
const char *password = "1werwerwer";       //WIFI jelszo
const char *mqtt_server = "192.168.43.42"; //Raspbery IP cime
WiFiClient espClient;
PubSubClient client(espClient);

char my_ip_address[4];
int my_IoT_id;
long lastMsg = 0;
char msg[50];
char msg1[50];
int value = 0;

char* devices[]={"humidity/DHT11/false","temperature/DHT11/false"};
//char* devices[]={"heatspeed/lampa/true","coolspead/ventilator/true"};

bool measure=false;
ICACHE_RAM_ATTR void IntCallback()
{
  if (lastisr + 5 < millis())
  {
    lastisr = millis();
    timerStateMachine = 0;
    if (heaterCounterValue != 0)
    {
      timer1_write(heaterCounterValue); //max 50000--> ez megfelel 10ms
    }
  }
  if (lastisr > millis() + 6)
  {
    lastisr = millis();
  }
}

ICACHE_RAM_ATTR void onTimerISR()
{
  if (timerStateMachine == 0)
  {
    digitalWrite(14, 1);
    timerStateMachine++;
    timer1_write(1000);
    return;
  }
  if (timerStateMachine == 1)
  {
    digitalWrite(14, 0);
  }
}

char *concat(const char *s1, const char *s2)
{
  Serial.println(s1);
  Serial.println(s2);
  char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
  // in real code you would check for errors in malloc here
  strcpy(result, s1);
  strcat(result, s2);
  Serial.println(result);
  return result;
}

void callback(char *topic, byte *payload, unsigned int length)
{
char *heat;
  char *startt;
  char *cool;
  char clientid[25];
  sprintf(clientid,"%d",ESP.getChipId());
  heat=concat(clientid,"/heatspeed");
  startt=concat(clientid,"/start");   
  cool=concat(clientid,"/coolspead");   



  int percentage;
  if (!strcmp(topic, heat))
  {
    char *c = (char *)malloc(sizeof(char) * (length + 1));
    for (int i = 0; i < length; i++)
    {
      c[i] = (char)payload[i];
    }
    c[length] = '\0';
    sscanf(c, "%i", &percentage);
    change_heating_speed(percentage);
    free(c);
  }

  if (!strcmp(topic, cool))
  {
    char *c = (char *)malloc(sizeof(char) * (length + 1));
    for (int i = 0; i < length; i++)
    {
      c[i] = (char)payload[i];
    }
    c[length] = '\0';
    sscanf(c, "%i", &percentage);

    free(c);
  }

  if(!strcmp(topic, startt))
  {
    Serial.println("start measure");
    measure=true;
  }

  free(heat);
  free(startt);
  free(cool);
}

void initProtocol()
{
  char ip_address[4];
  IPAddress ip = WiFi.localIP();

  strcpy(my_ip_address, ip_address);
  sprintf(ip_address, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  Serial.println(ip_address);


  char clientid[25];
  sprintf(clientid,"%d",ESP.getChipId());
  client.publish("/toserver/init/", clientid);
  Serial.println("sent chipId address");
  delay(1000);
  char *c;
  c=concat(clientid,"/myIp");
  client.publish(c, ip_address);
  free(c);
  Serial.println("sent IP address");

  for (size_t i = 0; i < 2; i++)
  {
    c=concat(clientid,"/devices");
    client.publish(c, devices[i]);
    free(c);
    Serial.println("sent device");
  }
  


}

void change_heating_speed(int percentage)
{
  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;
  if (percentage == 0)
  {
    heaterCounterValue = 0;
  }
  else
  {
    heaterCounterValue = 49000 - (480 * percentage);
  }
  Serial.print("HS===");
  Serial.println(heaterCounterValue);
}

void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  char *c;

  char startt[40];
  char heat[40];
  char cool[40];
  
  char cid[25];
  sprintf(cid,"%d",ESP.getChipId());
 
  c=concat(cid,"/start");
  strcpy(startt, c);
  free(c);

  c=concat(cid,"/heatspeed");
  strcpy(heat, c);
  free(c);


  c=concat(cid,"/coolspead");
  strcpy(cool, c);
  free(c);
  
  // Loop until we're reconnected
 
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");

      client.subscribe(startt);
      client.subscribe(cool);
      client.subscribe(heat);


    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(500);
    }
  }  
}

void readDhtSensor()
{
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f))
  {
    //   Serial.println("Failed to read from DHT sensor!");
    strcpy(celsiusTemp, "Failed");
    strcpy(fahrenheitTemp, "Failed");
    strcpy(humidityTemp, "Failed");
  }
  else
  {
    // Computes temperature values in Celsius + Fahrenheit and Humidity
    float hic = dht.computeHeatIndex(t, h, false);
    dtostrf(hic, 6, 2, celsiusTemp);
    float hif = dht.computeHeatIndex(f, h);
    dtostrf(hif, 6, 2, fahrenheitTemp);
    dtostrf(h, 6, 2, humidityTemp);
    // You can delete the following Serial.print's, it's just for debugging purposes
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
    Serial.println(" *F");
  }

  if (isnan(t))
  {
    sprintf(msg, "NaN");
  }
  else
  {
    int homerseklet = (int)((t)*100);
    sprintf(msg, "%i.%i", homerseklet / 100, (homerseklet % 100));
  }

  if (isnan(h))
  {
    sprintf(msg1, "NaN");
  }
  else
  {
    int nedv = (int)((h)*100);
    sprintf(msg1, "%i.%i", nedv / 100, (nedv % 100));
  }

  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.print("Publish message: ");
  Serial.println(msg1);

  char *str;
  char cid[25];
  sprintf(cid,"%d",ESP.getChipId());
 
  str=concat(cid,"/temperature");
  client.publish(str, msg);
  free(str);

  str=concat(cid,"/humidity");
  client.publish(str, msg1);
  free(str);

  delay(5000);
}
void setup()
{
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);

  pinMode(14, OUTPUT);
  attachInterrupt(4, IntCallback, RISING); //interrupt

  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
void loop()
{
  if (!client.connected())
  {
    reconnect();
    initProtocol();
  }
  if(measure)
  {
    readDhtSensor();
  }
  client.loop();
}
