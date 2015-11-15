#include <SPI.h>
#include <Ethernet.h>
#include <math.h>

float persistence;
int octaves;

int pinR = 5;
int pinG = 6;
int pinB = 9;

bool mode = 0; // 0 - single color, 1 - noise, 2 - nothing yet

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#define STRING_BUFFER_SIZE 128
char buffer[STRING_BUFFER_SIZE];

EthernetServer server(80);

struct Color {
  int r;
  int g;
  int b;
};
Color currentColor;

struct HttpRequest {
  bool showPage;
  bool showNoise;
  bool error;
  int r;
  int g;
  int b;
};

void setup() {
  persistence = 0.25;
  octaves = 3;

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);

  Serial.begin(9600);
  /*while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }*/

  Serial.println("Searching IP...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  server.begin();

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
}

int readUntilNewLine(EthernetClient client) {
  Serial.println("readUntilNewLine");
  int bufindex = 0;

  buffer[0] = client.read();
  buffer[1] = client.read();
  bufindex = 2;

  char c;
  while (buffer[bufindex-2] != '\r' && buffer[bufindex-1] != '\n') { // read full row and save it in buffer
    c = client.read();

    if (bufindex < STRING_BUFFER_SIZE-1) {
      buffer[bufindex] = c;
    }

    bufindex++;
  }

  return bufindex + 1;
}

HttpRequest readRequest(EthernetClient client) {
  HttpRequest result;
  result.error = false;
  result.showPage = false;
  result.showNoise = false;

  if (client.connected() && client.available()) {
    int length = readUntilNewLine(client);

    buffer[length] = '\0';
    String line(buffer);

    Serial.print("first line: ");
    Serial.println(line);

    if (line.startsWith("GET /noise")) {
      result.showNoise = true;

      Serial.println("showNoise: 1");
    } else if (line.startsWith("GET")) {
      result.showPage = line.startsWith("GET / ");

      Serial.print("showPage: ");
      Serial.println(result.showPage);

      if (!result.showPage) {
        if (!isDigit(line[5])) { Serial.println("unknown request"); result.error = true; return result; }

        int nextSlash = line.indexOf('/', 6);
        if (nextSlash == -1) { Serial.println("second slash not found"); result.error = true; return result; }
        String rStr = line.substring(5, nextSlash);

        int previousSlash = nextSlash;
        nextSlash = line.indexOf('/', previousSlash+1);
        if (nextSlash == -1) { Serial.println("third slash not found"); result.error = true; return result; }
        String gStr = line.substring(previousSlash+1, nextSlash);

        previousSlash = nextSlash;
        nextSlash = line.indexOf('/', previousSlash+1);
        if (nextSlash == -1) { Serial.println("fourth slash not found"); result.error = true; return result; }
        String bStr = line.substring(previousSlash+1, nextSlash);

        Serial.print("r: ");
        Serial.print(rStr);
        Serial.print(" g: ");
        Serial.print(gStr);
        Serial.print(" b: ");
        Serial.println(bStr);

        result.r = rStr.toInt();
        result.g = gStr.toInt();
        result.b = bStr.toInt();
      }
    } else {
      Serial.println("only GET requests supported");
      result.error = true;
    }
  }

  return result;
}

void loop() {
  if (mode == 1) {
    setNoiseColor();
  }

  EthernetClient client = server.available();

  if (client) {
    Serial.println("new client");

    HttpRequest req = readRequest(client);

    if (req.error) {
      sendError(client);
    } else if (req.showPage) {
      website(client);
    } else if (req.showNoise) {
      mode = 1;
      redirectHome(client);
    } else {
      setColor(req.r, req.g, req.b);
      redirectHome(client);
    }

    // give the web browser time to receive the data
    delay(1);

    client.stop();
    Serial.println("client disconnected");
    Serial.println();
    Ethernet.maintain();
  }
}

void setColor(int r, int g, int b) {
  currentColor.r = r;
  currentColor.g = g;
  currentColor.b = b;

  mode = 0;

  updatePins();
}

void updatePins() {
  Serial.print("updatePins [ r: ");
  Serial.print(currentColor.r);
  Serial.print(" g: ");
  Serial.print(currentColor.g);
  Serial.print(" b: ");
  Serial.print(currentColor.b);
  Serial.println("]");

  analogWrite(pinR, currentColor.r);
  analogWrite(pinG, currentColor.g);
  analogWrite(pinB, currentColor.b);
}

void sendError(EthernetClient client) {
  Serial.println("Respond with 404");

  client.println("HTTP/1.1 404 Not Found");
  client.println("Connection: close");
}

void website(EthernetClient client) {
  Serial.println("Respond with webpage");

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<a href=\"/255/0/0/\">Red</a>");
  client.println("<a href=\"/0/255/0/\">Green</a>");
  client.println("<a href=\"/0/0/255/\">Blue</a>");
  client.println("<a href=\"/noise\">Noise</a>");
  client.println("<p>or got to /red/green/blue/ (replace color with value between 0-255) to set color.</p>");
  client.println("</html>");
}

void redirectHome(EthernetClient client) {
  Serial.println("Respond with redirect to home");

  client.println("HTTP/1.1 304 Go Back");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
}

void setNoiseColor() {
 float x1 = float(millis())/100.0f;

 wheel(int(PerlinNoise2(x1, 0,persistence,octaves)*128+128));

 updatePins();
}

void wheel(byte pos) {
  pos = 255 - pos;

  if(pos < 85) {
    currentColor.r = 255 - pos * 3;
    currentColor.g = 0;
    currentColor.b = pos * 3;
    return;
  }

  if(pos < 170) {
    pos -= 85;
    currentColor.r = 0;
    currentColor.g = pos * 3;
    currentColor.b = 255 - pos * 3;
    return;
  }

  pos -= 170;
  currentColor.r = pos * 3;
  currentColor.g = 255-pos*3;
  currentColor.b = 0;
  return;
}

float Noise2(float x, float y)
{
 long noise;
 noise = x + y * 57;
 noise = (noise << 13) ^ noise;
 //noise = pow(noise << 13,noise);
 return ( 1.0 - ( long(noise * (noise * noise * 15731L + 789221L) + 1376312589L) & 0x7fffffff) / 1073741824.0);
}

float SmoothNoise2(float x, float y)
{
 float corners, sides, center;
 corners = ( Noise2(x-1, y-1)+Noise2(x+1, y-1)+Noise2(x-1, y+1)+Noise2(x+1, y+1) ) / 16;
 sides   = ( Noise2(x-1, y)  +Noise2(x+1, y)  +Noise2(x, y-1)  +Noise2(x, y+1) ) /  8;
 center  =  Noise2(x, y) / 4;
 return (corners + sides + center);
}

float InterpolatedNoise2(float x, float y)
{
 float v1,v2,v3,v4,i1,i2,fractionX,fractionY;
 long longX,longY;

 longX = long(x);
 fractionX = x - longX;

 longY = long(y);
 fractionY = y - longY;

 v1 = SmoothNoise2(longX, longY);
 v2 = SmoothNoise2(longX + 1, longY);
 v3 = SmoothNoise2(longX, longY + 1);
 v4 = SmoothNoise2(longX + 1, longY + 1);

 i1 = Interpolate(v1 , v2 , fractionX);
 i2 = Interpolate(v3 , v4 , fractionX);

 return(Interpolate(i1 , i2 , fractionY));
}

float Interpolate(float a, float b, float x)
{
 //cosine interpolations
 return(CosineInterpolate(a, b, x));
}


float LinearInterpolate(float a, float b, float x)
{
 return(a*(1-x) + b*x);
}

float CosineInterpolate(float a, float b, float x)
{
 float ft = x * 3.1415927;
 float f = (1 - cos(ft)) * .5;

 return(a*(1-f) + b*f);
}

float PerlinNoise2(float x, float y, float persistance, int octaves)
{
 float frequency, amplitude;
 float total = 0.0;

 for (int i = 0; i <= octaves - 1; i++)
 {
   frequency = pow(2,i);
   amplitude = pow(persistence,i);

   total = total + InterpolatedNoise2(x * frequency, y * frequency) * amplitude;
 }

 return(total);
}

