/*
release 38: activate PWM outputs. Managed the RTC not working. Managed the router not working
release 39: added command to force the switch ON and OFF of the lighting
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <microDS3231.h>

#ifndef STASSID
#define STASSID "your_net"
#define STAPSK  "your_password"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const int PIN_LED_1 = 12;
const int PIN_LED_2 = 14;
const int PIN_LED_3 = 13;
const int PIN_LED_4 = 16;

int difference_between_targets;
int addr = 0;
int html_page = 0;
int par[72];
int hourRTC = 3;
int phase;
int lux = 0;
byte target[4];
byte night_lighting[4];
byte name_light[4];
byte previous_target[4];
byte instant_target[4];
byte pwm_light[4];
byte immediate_preset;
byte connection_timeout;
byte rtc_not_found = 0;
byte mode_selected = 0; // 0 normal - 1 lamp ON - 2 lamp OFF
float percentage_in_the_period;
String message;
String text_light[33];
unsigned long speed_to_target[4];
unsigned long prev_millis_second;
unsigned long prev_millis_light[4];
unsigned long prev_millis_minute;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);

MicroDS3231 rtc;

void setup()
{
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  pinMode(PIN_LED_3, OUTPUT);
  pinMode(PIN_LED_4, OUTPUT);
  analogWrite(PIN_LED_1, 254);
  analogWrite(PIN_LED_2, 255);
  analogWrite(PIN_LED_3, 255);
  analogWrite(PIN_LED_4, 255);
  
  Serial.begin(115200);
  delay(300);

  if (rtc.lostPower())
  { 
    rtc.setTime(BUILD_SEC, BUILD_MIN, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
  }

//Initialize EEPROM
  EEPROM.begin(512);

  // prepare LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);

  // Connect to WiFi network
  Serial.println();
  delay(500);
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while ((WiFi.status() != WL_CONNECTED) && (millis() < 60000))
  {
    delay(500);
    Serial.print(F("."));
  }

  if (millis() > 60000) // connection router timeout (after 60 seconds)
  {
    Serial.println();
    Serial.println(F("WiFi NOT connected"));
    connection_timeout = 1;
  }
  else // connection on time
  {
    Serial.println();
    Serial.println(F("WiFi connected"));
    digitalWrite(LED_BUILTIN, 1);
  
    if (MDNS.begin("plafo")) {
      Serial.println("MDNS responder started.      HTTP://plafo.local/");
    }
  
    server.onNotFound(handleWebPage);
  
    // Start the server
    server.begin();
    Serial.println(F("Server started"));
  
    // Print the IP address
    Serial.println(WiFi.localIP());
  }

  // Read EEPROM
  read_eeprom();

  if (!rtc.begin())
  {
    rtc_not_found = 1;
    Serial.println("DS3231 not found");
  }

  // Read RTC
  if (!rtc_not_found) hourRTC = rtc.getHours() * 60;
  if (!rtc_not_found) hourRTC += rtc.getMinutes();

  // Set the output after the first start (depending by the RTC and the parametrization)
  immediate_preset = 1;
  prev_millis_light[0] = 0;
  prev_millis_light[1] = 0;
  prev_millis_light[2] = 0;
  prev_millis_light[3] = 0;
  text_light[0] = F("WHITE");
  text_light[1] = F("WHITE 1");
  text_light[2] = F("WHITE 2");
  text_light[3] = F("WHITE 3");
  text_light[4] = F("WHITE 4");
  text_light[5] = F("DIFFUSE WHITE");
  text_light[6] = F("WARM");
  text_light[7] = F("WARM 1");
  text_light[8] = F("WARM 2");
  text_light[9] = F("COLD");
  text_light[10] = F("COLD 1");
  text_light[11] = F("COLD 2");
  text_light[12] = F("BLUE");
  text_light[13] = F("BLUE 1");
  text_light[14] = F("BLUE 2");
  text_light[15] = F("BLUE 3");
  text_light[16] = F("BLUE 4");
  text_light[17] = F("DIFFUSE BLUE");
  text_light[18] = F("RED");
  text_light[19] = F("RED 1");
  text_light[20] = F("RED 2");
  text_light[21] = F("VIOLET");
  text_light[22] = F("VIOLET 1");
  text_light[23] = F("VIOLET 2");
  text_light[24] = F("ACTINIC");
  text_light[25] = F("ACTINIC 1");
  text_light[26] = F("ACTINIC 2");
  text_light[27] = F("UV");
  text_light[28] = F("UV 1");
  text_light[29] = F("UV 2");
  text_light[30] = F("GREEN");
  text_light[31] = F("GREEN 1");
  text_light[32] = F("GREEN 2");
}

void loop()
{
  server.handleClient();
  MDNS.update();
  lighting_program_check();
  pwm_managing();
}

void handleWebPage()
{
  // Read the request
  String req = server.uri();

  // Match the request
  if (req.indexOf(F("/setup")) != -1)
  {
    html_page = 0;
    if (req.indexOf(F("k")) != -1)
    {
      String search_parameter = F("");
      for (byte n = 0; n < 4; n++)
      {
        name_light[n] = server.arg(n).toInt();
      }
    }
    else
    {
      message = F("Setting ...");
    }
  }
  else if (req.indexOf(F("L1")) != -1)
  {
    html_page = 1;
    message = F("Setting light ");
    message += text_light[name_light[0]];
  }
  else if (req.indexOf(F("L2")) != -1)
  {
    html_page = 2;
    message = F("Setting light ");
    message += text_light[name_light[1]];
  }
  else if (req.indexOf(F("L3")) != -1)
  {
    html_page = 3;
    message = F("Setting light ");
    message += text_light[name_light[2]];
  }
  else if (req.indexOf(F("L4")) != -1)
  {
    html_page = 4;
    message = F("Setting light ");
    message += text_light[name_light[3]];
  }
  else if (req.indexOf(F("preset")) != -1)
  {
    preset();
    message = F("Preset done!");
  }
  else if (req.indexOf(F("sa")) != -1)
  {
    save();
    message = F("Saved!");
  }
  else if (req.indexOf(F("jnorm")) != -1)
  {
    mode_selected = 0;
  }
  else if (req.indexOf(F("jon")) != -1)
  {
    mode_selected = 1;
  }
  else if (req.indexOf(F("joff")) != -1)
  {
    mode_selected = 2;
  }

  addr = 0;
  if (req.indexOf(F("h")) != -1)       {addr = 0 + ((html_page - 1) * 18);}
  else if (req.indexOf(F("d")) != -1)  {addr = 1 + ((html_page - 1) * 18);}
  else if (req.indexOf(F("v")) != -1)  {addr = 2 + ((html_page - 1) * 18);}
  if (req.indexOf(F("11")) != -1)      {addr += 0;}
  else if (req.indexOf(F("12")) != -1) {addr += 3;}
  else if (req.indexOf(F("13")) != -1) {addr += 6;}
  else if (req.indexOf(F("14")) != -1) {addr += 9;}
  else if (req.indexOf(F("15")) != -1) {addr += 12;}
  else if (req.indexOf(F("16")) != -1) {addr += 15;}

  if (req.indexOf(F("p01")) != -1)      // request to add 1
  {
    if (req.indexOf(F("h")) != -1)      // request to add one to the hour
    {
      if (par[addr] < 1439)             // the hour will not exceed midnight 
      {
        if (((addr + 3) % 18) == 0)     // last period
        {
          if ((par[addr] + par[addr+1] + 1) <= 1440) // the end of the phase does not exceed midnight
          {
            par[addr] += 1;
          }
          else                                       // the end of the phase exceeds midnight
          {
            if ((par[addr] + par[addr+1] + 1 - 1440) <= par[addr-15]) // the end of the phase will not exceed the start of the next
            {
              par[addr] += 1;
            }
            else                                                      // the end of the phase will exceed the start of the next
            {
              message = F("The end of this phase cannot exceed the beginning of the next!");
            }
          }
        }
        else                            // not the last period
        {
          if ((par[addr] + par[addr+1]) < par[addr+3]) // the end of the phase will not exceed the start of the next
          {
            par[addr] += 1;
          }
          else                                         // the end of the phase will surpass the beginning of the next
          {
            message = F("The end of this phase cannot exceed the beginning of the next!");
          }
        }
      }
      else                              // the hour will exceed midnight 
      {
        message = F("The start of this phase cannot exceed midnight!");
      }
    }
    else if (req.indexOf(F("d")) != -1) // request to add one to the duration
    {
      if (par[addr] < 240)              // the duration will not exceed 240 minutes
      {
        if (((addr + 2) % 18) == 0)     // last period
        {
          if ((par[addr-1] + par[addr] + 1) <= 1440) // the end of the phase does not exceed midnight
          {
            par[addr] += 1;
          }
          else                                       // the end of the phase exceeds midnight
          {
            if ((par[addr-1] + par[addr] + 1 - 1440) <= par[addr-16]) // the end of the phase will not exceed the start of the next
            {
              par[addr] += 1;
            }
            else                                                     // the end of the phase will exceed the start of the next
            {
              message = F("The end of this phase cannot exceed the beginning of the next!");
            }
          }
        }
        else                            // not the last period
        {
          if ((par[addr-1] + par[addr]) < par[addr+2]) // the end of the phase will not exceed the start of the next
          {
            par[addr] += 1;
          }
          else                                         // the end of the phase will surpass the beginning of the next
          {
            message = F("The end of this phase cannot exceed the beginning of the next!");
          }
        }
      }
      else                              // the duration will exceed 240 minutes
      {
        message = F("The duration of this phase cannot exceed 240 minutes!");
      }
    }
    else if (req.indexOf(F("v")) != -1) // request to add one to the value
    {
      if (par[addr] < 100)              // value less of 100
      {
        par[addr] += 1;
      }
      else                              // value equal to 100
      {
        message = F("The value cannot exceed 100%!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to add one to the clock
    {
      hourRTC += 1;
      if (hourRTC >= 1440)
      {
        hourRTC -= 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
    else if (req.indexOf(F("n")) != -1) // request to set night lighting
    {
      night_lighting[(html_page - 1)] = 1;
    }
  }
  else if (req.indexOf(F("p10")) != -1) // request to add 10
  {
    if (req.indexOf(F("h")) != -1)      // request to add ten to the hour
    {
      if (par[addr] < 1430)             // the hour will not exceed midnight 
      {
        if (((addr + 3) % 18) == 0)     // last period
        {
          if ((par[addr] + par[addr+1] + 10) <= 1440) // the end of the phase does not exceed midnight
          {
            par[addr] += 10;
          }
          else                                        // the end of the phase exceeds midnight
          {
            if ((par[addr] + par[addr+1] + 10 - 1440) <= par[addr-15]) // the end of the phase will not exceed the start of the next
            {
              par[addr] += 10;
            }
            else                                                     // the end of the phase will exceed the start of the next
            {
              message = F("The end of this phase cannot exceed the beginning of the next!");
            }
          }
        }
        else                            // not the last period
        {
          if (((par[addr] + par[addr+1]) + 10) <= par[addr+3]) // the end of the phase will not exceed the start of the next
          {
            par[addr] += 10;
          }
          else                                         // the end of the phase will surpass the beginning of the next
          {
            message = F("The end of this phase cannot exceed the beginning of the next!");
          }
        }
      }
      else                              // the hour will exceed midnight 
      {
        message = F("The start of this phase cannot exceed midnight!");
      }
    }
    else if (req.indexOf(F("d")) != -1) // request to add ten to the duration
    {
      if (par[addr] < 231)              // the duration will not exceed 240 minutes
      {
        if (((addr + 2) % 18) == 0)     // last period
        {
          if ((par[addr-1] + par[addr] + 10) <= 1440) // the end of the phase does not exceed midnight
          {
            par[addr] += 10;
          }
          else                                       // the end of the phase exceeds midnight
          {
            if ((par[addr-1] + par[addr] + 10 - 1440) <= par[addr-16]) // the end of the phase will not exceed the start of the next
            {
              par[addr] += 10;
            }
            else                                                     // the end of the phase will exceed the start of the next
            {
              message = F("The end of this phase cannot exceed the beginning of the next!");
            }
          }
        }
        else                            // not the last period
        {
          if (((par[addr-1] + par[addr]) + 10) <= par[addr+2]) // the end of the phase will not exceed the start of the next
          {
            par[addr] += 10;
          }
          else                                                // the end of the phase will surpass the beginning of the next
          {
            message = F("The end of this phase cannot exceed the beginning of the next!");
          }
        }
      }
      else                              // the duration will exceed 240 minutes
      {
        message = F("The duration of this phase cannot exceed 240 minutes!");
      }
    }
    else if (req.indexOf(F("v")) != -1) // request to add ten to the value
    {
      if (par[addr] <= 90)               // value less of 90
      {
        par[addr] += 10;
      }
      else                              // value over 90
      {
        message = F("The value cannot exceed 100%!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to add ten to the clock
    {
      hourRTC += 10;
      if (hourRTC >= 1440)
      {
        hourRTC -= 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
  }
  else if (req.indexOf(F("p60")) != -1) // request to add 60
  {
    if (req.indexOf(F("h")) != -1)      // request to add sixty to the hour
    {
      if (par[addr] < 1380)             // the hour will not exceed midnight 
      {
        if (((addr + 3) % 18) == 0)     // last period
        {
          if ((par[addr] + par[addr+1] + 60) <= 1440) // the end of the phase does not exceed midnight
          {
            par[addr] += 60;
          }
          else                                        // the end of the phase exceeds midnight
          {
            if ((par[addr] + par[addr+1] + 60 - 1440) <= par[addr-15]) // the end of the phase will not exceed the start of the next
            {
              par[addr] += 60;
            }
            else                                                     // the end of the phase will exceed the start of the next
            {
              message = F("The end of this phase cannot exceed the beginning of the next!");
            }
          }
        }
        else                            // not the last period
        {
          if (((par[addr] + par[addr+1]) + 60) <= par[addr+3]) // the end of the phase will not exceed the start of the next
          {
            par[addr] += 60;
          }
          else                                         // the end of the phase will surpass the beginning of the next
          {
            message = F("The end of this phase cannot exceed the beginning of the next!");
          }
        }
      }
      else                              // the hour will exceed midnight 
      {
        message = F("The start of this phase cannot exceed midnight!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to add sixty to the clock
    {
      hourRTC += 60;
      if (hourRTC >= 1440)
      {
        hourRTC -= 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
  }
  else if (req.indexOf(F("m01")) != -1) // request to subtract 1
  {
    if (req.indexOf(F("h")) != -1)      // request to subtract 1 to the hour
    {
      if (par[addr] > 0)                // the hour will not drop below midnight 
      {
        if ((addr % 18) == 0)           // first period
        {
          if (par[addr+15] + par[addr+16] <= 1440) // the last period ends before midnight
          {
            par[addr] -= 1;
          }
          else                                     // the last period ends after midnight
          {
            if (par[addr] > (par[addr+15] + par[addr+16] - 1440)) // the period begins after the end of the previous one
            {
              par[addr] -= 1;
            }
            else                                                  // the period would start before the end of the previous one
            {
              message = F("The beginning of this phase cannot be before the end of the previous one!");
            }
          }
        }
        else                            // not first period
        {
          if (par[addr] > (par[addr-3] + par[addr-2])) // the period begins after the end of the previous one 
          {
            par[addr] -= 1;
          }
          else                                         // the period would start before the end of the previous one
          {
            message = F("The beginning of this phase cannot be before the end of the previous one!");
          }
        }
      }
      else                              // the hour will drop below midnight
      {
        message = F("The start of this phase cannot be before midnight!");
      }
    }
    else if (req.indexOf(F("d")) != -1) // request to subtract 1 to the duration
    {
      if (par[addr] > 0)                // duration greater than 0
      {
        par[addr] -= 1;
      }
      else                              // duration equal to 0
      {
        message = F("The duration cannot be less than 0!");
      }
    }
    else if (req.indexOf(F("v")) != -1) // request to subtract 1 to the value
    {
      if (par[addr] > 0)                // value greater than 0
      {
        par[addr] -= 1;
      }
      else                              // value equal to 0
      {
        message = F("The value cannot be less than 0!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to subctract one to the clock
    {
      hourRTC -= 1;
      if (hourRTC < 0)
      {
        hourRTC += 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
    else if (req.indexOf(F("n")) != -1) // request to reset night lighting
    {
      night_lighting[(html_page - 1)] = 0;
    }
  }
  else if (req.indexOf(F("m10")) != -1) // request to subtract 10
  {
    if (req.indexOf(F("h")) != -1)      // request to subtract ten to the hour
    {
      if (par[addr] > 9)                // the hour will not drop below midnight 
      {
        if ((addr % 18) == 0)           // first period
        {
          if (par[addr+15] + par[addr+16] <= 1440) // the last period ends before midnight
          {
            par[addr] -= 10;
          }
          else                                     // the last period ends after midnight
          {
            if ((par[addr] - 9) > (par[addr+15] + par[addr+16] - 1440)) // the period begins after the end of the previous one
            {
              par[addr] -= 10;
            }
            else                                                  // the period would start before the end of the previous one
            {
              message = F("The beginning of this phase cannot be before the end of the previous one!");
            }
          }
        }
        else                            // not first period
        {
          if ((par[addr] - 9) > (par[addr-3] + par[addr-2])) // the period begins after the end of the previous one 
          {
            par[addr] -= 10;
          }
          else                                         // the period would start before the end of the previous one
          {
            message = F("The beginning of this phase cannot be before the end of the previous one!");
          }
        }
      }
      else                              // the hour will drop below midnight
      {
        message = F("The start of this phase cannot be before midnight!");
      }
    }
    else if (req.indexOf(F("d")) != -1) // request to subtract ten to the duration
    {
      if (par[addr] > 9)                // duration greater than 0
      {
        par[addr] -= 10;
      }
      else                              // duration equal to 0
      {
        message = F("The duration cannot be less than 0!");
      }
    }
    else if (req.indexOf(F("v")) != -1) // request to subtract ten to the value
    {
      if (par[addr] > 9)                // value greater than 0
      {
        par[addr] -= 10;
      }
      else                              // value equal to 0
      {
        message = F("The value cannot be less than 0!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to subctract ten to the clock
    {
      hourRTC -= 10;
      if (hourRTC < 0)
      {
        hourRTC += 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
  }
  else if (req.indexOf(F("m60")) != -1) // request to subtract 60
  {
    if (req.indexOf(F("h")) != -1)      // request to subtract sixty to the hour
    {
      if (par[addr] > 59)                // the hour will not drop below midnight 
      {
        if ((addr % 18) == 0)           // first period
        {
          if (par[addr+15] + par[addr+16] <= 1440) // the last period ends before midnight
          {
            par[addr] -= 60;
          }
          else                                     // the last period ends after midnight
          {
            if ((par[addr] - 59) > (par[addr+15] + par[addr+16] - 1440)) // the period begins after the end of the previous one
            {
              par[addr] -= 60;
            }
            else                                                  // the period would start before the end of the previous one
            {
              message = F("The beginning of this phase cannot be before the end of the previous one!");
            }
          }
        }
        else                            // not first period
        {
          if ((par[addr] - 59) > (par[addr-3] + par[addr-2])) // the period begins after the end of the previous one 
          {
            par[addr] -= 60;
          }
          else                                         // the period would start before the end of the previous one
          {
            message = F("The beginning of this phase cannot be before the end of the previous one!");
          }
        }
      }
      else                              // the hour will drop below midnight
      {
        message = F("The start of this phase cannot be before midnight!");
      }
    }
    else if (req.indexOf(F("c")) != -1) // request to subcratct sixty to the clock
    {
      hourRTC -= 60;
      if (hourRTC < 0)
      {
        hourRTC += 1440;
      }
      rtc.setTime(0, (hourRTC % 60), (hourRTC / 60), BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    }
  }

  String stringHTML = F("<html>");
  stringHTML += F("<head>");
  stringHTML += F("</head>");
  stringHTML += F("<body>");
  stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 10px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"1\" cellspacing=\"0\" cellpadding=\"0\">");
  stringHTML += F("<tbody>");
  stringHTML += F("<tr>");
  stringHTML += F("<td style=\"width: 100%; background-color: #c3c3c3; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
  stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 10px; border-style: none; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">");
  stringHTML += F("<tbody>");
  stringHTML += F("<tr>");
  stringHTML += F("<td style=\"width: 20%; text-align: center; vertical-align: middle;\"><a title=\"Setup\" style=\"text-decoration: none\" href=\"http:/setup\">");
  if (html_page == 0)
  {
    stringHTML += F("<span style=\"font-size: 24pt;\"><font color=\"red\"><strong>SETUP</font></strong></span>");
  }
  else
  {
    stringHTML += F("<span style=\"font-size: 24pt;\">SETUP</span>");
  }
  stringHTML += F("</a></td>");
  stringHTML += F("<td style=\"width: 20%; text-align: center; vertical-align: middle;\"><a title=\"");
  stringHTML += text_light[name_light[0]];
  stringHTML += F("\" style=\"text-decoration: none\" href=\"http:/L1\">");
  if (html_page == 1)
  {
    stringHTML += F("<span style=\"font-size: 24pt;\"><font color=\"red\"><strong>");
    stringHTML += text_light[name_light[0]];
    stringHTML += F("</font></strong></span>");
  }
  else
  {
    stringHTML += F("<span style=\"font-size: 24pt;\">");
    stringHTML += text_light[name_light[0]];
    stringHTML += F("</span>");
  }
  stringHTML += F("</a></td>");
  stringHTML += F("<td style=\"width: 20%; text-align: center; vertical-align: middle;\"><a title=\"");
  stringHTML += text_light[name_light[1]];
  stringHTML += F("\" style=\"text-decoration: none\" href=\"http:/L2\">");
  if (html_page == 2)
  {
    stringHTML += F("<span style=\"font-size: 24pt;\"><font color=\"red\"><strong>");
    stringHTML += text_light[name_light[1]];
    stringHTML += F("</font></strong></span>");
  }
  else
  {
    stringHTML += F("<span style=\"font-size: 24pt;\">");
    stringHTML += text_light[name_light[1]];
    stringHTML += F("</span>");
  }
  stringHTML += F("</a></td>");
  stringHTML += F("<td style=\"width: 20%; text-align: center; vertical-align: middle;\"><a title=\"");
  stringHTML += text_light[name_light[2]];
  stringHTML += F("\" style=\"text-decoration: none\" href=\"http:/L3\">");
  if (html_page == 3)
  {
    stringHTML += F("<span style=\"font-size: 24pt;\"><font color=\"red\"><strong>");
    stringHTML += text_light[name_light[2]];
    stringHTML += F("</font></strong></span>");
  }
  else
  {
    stringHTML += F("<span style=\"font-size: 24pt;\">");
    stringHTML += text_light[name_light[2]];
    stringHTML += F("</span>");
  }
  stringHTML += F("</a></td>");
  stringHTML += F("<td style=\"width: 20%; text-align: center; vertical-align: middle;\"><a title=\"");
  stringHTML += text_light[name_light[3]];
  stringHTML += F("\" style=\"text-decoration: none\" href=\"http:/L4\">");
  if (html_page == 4)
  {
    stringHTML += F("<span style=\"font-size: 24pt;\"><font color=\"red\"><strong>");
    stringHTML += text_light[name_light[3]];
    stringHTML += F("</font></strong></span>");
  }
  else
  {
    stringHTML += F("<span style=\"font-size: 24pt;\">");
    stringHTML += text_light[name_light[3]];
    stringHTML += F("</span>");
  }
  stringHTML += F("</a></td>");
  stringHTML += F("</tr>");
  stringHTML += F("</tbody>");
  stringHTML += F("</table>");
  stringHTML += F("</td>");
  stringHTML += F("</tr>");
  if (html_page != 0)
  {
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>1</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[0 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[0 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[0 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[0 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[1 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[0 + ((html_page - 1) * 18)] + par[1 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[2 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h11p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h11p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h11p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h11m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h11m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h11m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d11p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d11p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d11m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d11m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v11p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v11p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v11m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v11m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>2</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[3 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[3 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[3 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[3 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[4 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[3 + ((html_page - 1) * 18)] + par[4 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[5 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h12p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h12p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h12p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h12m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h12m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h12m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d12p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d12p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d12m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d12m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v12p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v12p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v12m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v12m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>3</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[6 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[6 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[6 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[6 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[7 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[6 + ((html_page - 1) * 18)] + par[7 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[8 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h13p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h13p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h13p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h13m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h13m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h13m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d13p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d13p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d13m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d13m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v13p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v13p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v13m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v13m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>4</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[9 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[9 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[9 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[9 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[10 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[9 + ((html_page - 1) * 18)] + par[10 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[11 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h14p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h14p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h14p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h14m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h14m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h14m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d14p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d14p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d14m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d14m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v14p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v14p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v14m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v14m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>5</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[12 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[12 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[12 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[12 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[13 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[12 + ((html_page - 1) * 18)] + par[13 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[14 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h15p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h15p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h15p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h15m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h15m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h15m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d15p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d15p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d15m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d15m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v15p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v15p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v15m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v15m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; text-align: center; vertical-align: middle; border: 1px solid #c3c3c3;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 44px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 10%; height: 44px; text-align: center; vertical-align: middle;\" rowspan=\"2\"><span style=\"font-size: 36pt;\"><strong>6</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Hour: <strong>");
    if (((par[15 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[15 + ((html_page - 1) * 18)]) / 60;
    stringHTML += F(":");
    if (((par[15 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
    stringHTML += (par[15 + ((html_page - 1) * 18)]) % 60;
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Duration: <strong>");
    stringHTML += par[16 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> min</br>End of the transition: <strong>");
    if (par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)] < 1440)
    {
      if (((par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)]) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)]) / 60;
      stringHTML += F(":");
      if (((par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)]) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)]) % 60;
    }
    else
    {
      if (((par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)] - 1440) / 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)] - 1440) / 60;
      stringHTML += F(":");
      if (((par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)] - 1440) % 60) < 10) {stringHTML += F("0");}
      stringHTML += (par[15 + ((html_page - 1) * 18)] + par[16 + ((html_page - 1) * 18)] -1440) % 60;
    }
    stringHTML += F("</strong></span></td>");
    stringHTML += F("<td style=\"width: 30%; height: 22px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Value: <strong>");
    stringHTML += par[17 + ((html_page - 1) * 18)];
    stringHTML += F("</strong> %</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr style=\"height: 22px;\">");
    stringHTML += F("<td style=\"width: 34%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/h16p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/h16p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/h16p60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/h16m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/h16m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/h16m60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/d16p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/d16p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/d16m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/d16m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("<td style=\"width: 28%; height: 22px; text-align: center; vertical-align: middle;\"><a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/v16p01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/v16p10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/v16m01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/v16m10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 30%; text-align: center; vertical-align: middle; height: 22px;\"><span style=\"font-size: 18pt;\">Use as night lighting: <strong>");
    if (night_lighting[(html_page - 1)] == 1)
    {
      stringHTML += F("YES"); 
    }
    else
    {
      stringHTML += F("NO"); 
    }
    stringHTML += F("</strong></span> <a title=\"KEEP ON\" style=\"text-decoration: none\"  href=\"http:/np01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(KEEP ON)</strong></font></span></a> <a title=\"KEEP OFF\" style=\"text-decoration: none\"  href=\"http:/nm01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(KEEP OFF)</strong></font></span></a>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
  }
  else
  {
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 100%; border: 1px solid #c3c3c3; text-align: center; vertical-align: middle;\">");
    stringHTML += F("<table style=\"border-collapse: collapse; width: 100%; height: 10px; border-color: #c3c3c3; border-style: solid; margin-left: auto; margin-right: auto;\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
    stringHTML += F("<tbody>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><a title=\"Save\" style=\"text-decoration: none\" href=\"http:/sa\"><span style=\"font-size: 24pt;\">SAVE</span></a></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Save the parametrization</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><a title=\"Preset\" style=\"text-decoration: none\" href=\"http:/preset\"><span style=\"font-size: 24pt;\">PRESET</span></a></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Sets the default values for a 10 hours photoperiod starting at 1:00 PM with an hour sunrise and sunset</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 24pt;\">");
    if ((hourRTC / 60) < 10) {stringHTML += F("0");}
    stringHTML += hourRTC / 60;
    stringHTML += F(":");
    if ((hourRTC % 60) < 10) {stringHTML += F("0");}
    stringHTML += hourRTC % 60;
    stringHTML += F("</span></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\">");
    if (rtc_not_found == 1)
    {
      stringHTML += F("<span style=\"font-size: 18pt;\"><font color=\"red\">Internal clock not working (check the battery) </font></span>");
    }
    stringHTML += F("<span style=\"font-size: 18pt;\">Sets the Clock </span>");
    stringHTML += F("<a title=\"+1\" style=\"text-decoration: none\"  href=\"http:/cp01\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+1)</strong></font></span></a> <a title=\"+10\" style=\"text-decoration: none\"  href=\"http:/cp10\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+10)</strong></font></span></a> <a title=\"+60\" style=\"text-decoration: none\"  href=\"http:/cp60\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(+60)</strong></font></span></a> <a title=\"-1\" style=\"text-decoration: none\"  href=\"http:/cm01\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-1)</strong></font></span></a> <a title=\"-10\" style=\"text-decoration: none\"  href=\"http:/cm10\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-10)</strong></font></span></a> <a title=\"-60\" style=\"text-decoration: none\"  href=\"http:/cm60\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(-60)</strong></font></span></a></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 24pt;\">NAMES</span></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Choose the name of the four lamps </span>");
    stringHTML += F("<form action=\"/setup/k\">");
    for (byte n = 0; n < 4; n++)
    {
      stringHTML += F("<select name=\"k");
      stringHTML += n;
      stringHTML += F("\" id=\"k");
      stringHTML += n;
      stringHTML += F("\">");
      for (byte i = 0; i < 33; i++)
      {
        stringHTML += F("<option value=\"");
        stringHTML += i;
        stringHTML += F("\"");
        if (name_light[n] == i)
        {
          stringHTML += F(" selected");
        }
        stringHTML += F(">");

        stringHTML += text_light[i];
        stringHTML += F("</option>");
      } 
      stringHTML += F("</select> ");
    }
    stringHTML += F("<input type=\"submit\" value=\" DONE ! \">");
    stringHTML += F("</form>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 24pt;\">OUT</span></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">");
    stringHTML += F("Power outputs ");
    for (byte i = 0; i < 4; i++)
    {
     stringHTML += F("(");
     stringHTML += text_light[name_light[i]];
     stringHTML += F("=");
     stringHTML += String((pwm_light[i] + 2) * 100 / 255);
     stringHTML += F("%) ");
    } 
    stringHTML += F("</span></td>");
    stringHTML += F("</tr>");
    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 24pt;\">");
    stringHTML += String(rtc.getTemperatureFloat()).substring(0, 4);
    stringHTML += char(176);
    stringHTML += F("C</span></td>");
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Temperature inside the light controller</span></td>");
    stringHTML += F("</tr>");

    stringHTML += F("<tr>");
    stringHTML += F("<td style=\"width: 15%; height: 10px; text-align: center; vertical-align: middle;\"><span style=\"font-size: 24pt;\">MODE</span></td>");

   
    
    stringHTML += F("<td style=\"width: 85%; height: 10px; text-align: left; vertical-align: middle;\"><span style=\"font-size: 18pt;\">Model selected: <strong>");
    if (mode_selected == 0)
    {
      stringHTML += F("NORMAL"); 
    }
    else if (mode_selected == 1)
    {
      stringHTML += F("FORCED ON"); 
    }
    else if (mode_selected == 2)
    {
      stringHTML += F("FORCED OFF"); 
    }
    stringHTML += F("<strong></span> <a title=\"FORCE ON\" style=\"text-decoration: none\"  href=\"http:/jon\"><span style=\"font-size: 18pt;\"><font color=\"green\"><strong>(FORCE ON)</strong></font></span></a> <a title=\"NORMAL\" style=\"text-decoration: none\"  href=\"http:/jnorm\"><span style=\"font-size: 18pt;\"><font color=\"blue\"><strong>(NORMAL)</strong></font></span></a> <a title=\"FORCE OFF\" style=\"text-decoration: none\"  href=\"http:/joff\"><span style=\"font-size: 18pt;\"><font color=\"red\"><strong>(FORCE OFF)</strong></font></span></a></td></tr>");
    stringHTML += F("</tbody>");
    stringHTML += F("</table>");
    stringHTML += F("</td>");
    stringHTML += F("</tr>");
  }
  stringHTML += F("<tr>");
  stringHTML += F("<td style=\"width: 100%; border: 1px solid #c3c3c3; text-align: center; vertical-align: middle;\"><span style=\"font-size: 18pt; color: #e03e2d;\">");
  stringHTML += message;
  message = F("");
  stringHTML += F("</span></td>");
  stringHTML += F("</tr>");
  stringHTML += F("</tbody>");
  stringHTML += F("</table>");
  stringHTML += F("</body>");
  stringHTML += F("</html>");
  server.send(200, "text/html", stringHTML);
}


void lighting_program_check()
{
  for (lux = 0; lux < 4; lux++)
  {
    for (phase = 11; phase >= 0; phase--)
    {
      if ((phase % 2) == 1) // check if the hour is after the end of a period If yes exit
      {
        if (hourRTC >= (par[(phase / 2 * 3) + (18 * lux)] + par[((phase / 2 * 3) + 1) + (18 * lux)]))         
        {
          break;
        }
      }
      else                  // check if the hour is in middle of a period. If yes exit
      {
        if (hourRTC >= (par[(phase / 2 * 3) + (18 * lux)]))         
        {
          if ((phase == 10) && ((par[15 + (18 * lux)] + par[16 + (18 * lux)]) >= 1440)) // check if the hour is in middle of the last period and it ends after midnight
          {
            phase = 15;
            break;
            
          }
          break;
        }
      }
    }
    if (phase == -1)
    {
      if (par[15 + (18 * lux)] + par[16 + (18 * lux)] < 1440) // check if the last period ends before midnight. if yes phase=12
      {
        phase = 12;
      }
      else                          // the last period ends after midnight and it is not finished 
      {
        if (hourRTC < (par[15 + (18 * lux)] + par[16 + (18 * lux)] - 1440)) // check il the last period, that it start the day before, is not finish
        {
          phase = 13;
        }
        else                        // the last period, that it start the day before, is finish
        {
          phase = 14;
        }
      }
    }
    target[lux] = 0;
    previous_target[lux] = 0;
    instant_target[lux] = 0;
    speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
    difference_between_targets = 0;
    percentage_in_the_period = 0;
    if (phase == 12) // the last period ends before midnight and is not starter the first period
    {
      target[lux] = par[17 + (18 * lux)] * 2.55F;
      instant_target[lux] = target[lux];
      speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
    }
    else if (phase == 13) // the last period started the day before, is not finish
    {
      target[lux] = par[17 + (18 * lux)] * 2.55F;
      previous_target[lux] = par[14 + (18 * lux)] * 2.55F;
      difference_between_targets = target[lux] - previous_target[lux];
      percentage_in_the_period = par[16 + (18 * lux)]; // duration
      percentage_in_the_period = (hourRTC + (1440 - par[15 + (18 * lux)])) / percentage_in_the_period; // minutes passed / duration
      instant_target[lux] = previous_target[lux] + (difference_between_targets * percentage_in_the_period);
      if (difference_between_targets < 0)
      {
        difference_between_targets = 0 - difference_between_targets;
      }
      else if (difference_between_targets == 0)
      {
        difference_between_targets = 1;
      }
      if (par[16] > 0) // if duration > 0 will be removed 2,5 seconds
      {
        speed_to_target[lux] = ((par[16] * 60 * 1000) - 2500) / difference_between_targets;
      }
      else
      {
        speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
      }
    }
    else if (phase == 14) // the last period started the day before, is finish (but is not starter the first period)
    {
      target[lux] = par[17 + (18 * lux)] * 2.55F;
      instant_target[lux] = target[lux];
      speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
    }
    else if (phase == 15) // the last period in not finish and it ends after midnight
    {
      target[lux] = par[17 + (18 * lux)] * 2.55F;
      previous_target[lux] = par[14 + (18 * lux)] * 2.55F;
      difference_between_targets = target[lux] - previous_target[lux];
      percentage_in_the_period = par[16 + (18 * lux)]; // duration
      percentage_in_the_period = (hourRTC - par[15 + (18 * lux)]) / percentage_in_the_period; // minutes passed / duration
      instant_target[lux] = previous_target[lux] + (difference_between_targets * percentage_in_the_period);
      if (difference_between_targets < 0)
      {
        difference_between_targets = 0 - difference_between_targets;
      }
      else if (difference_between_targets == 0)
      {
        difference_between_targets = 1;
      }
      if (par[16] > 0) // if duration > 0 will be removed 2,5 seconds
      {
        speed_to_target[lux] = ((par[16] * 60 * 1000) - 2500) / difference_between_targets;
      }
      else
      {
        speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
      }
    }
    else if ((phase % 2) == 1) // a period is finish
    {
      target[lux] = par[((phase / 2 * 3) + 2) + (18 * lux)] * 2.55F;
      instant_target[lux] = target[lux];
      speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
    }
    else // a period is not finish
    {
      target[lux] = par[((phase / 2 * 3) + 2) + (18 * lux)] * 2.55F;
      if ((phase / 2) == 0) // first phase
      {
        previous_target[lux] = par[17 + (18 * lux)] * 2.55F;;
      }
      else                  // not first phase
      {
        previous_target[lux] = par[((((phase / 2) - 1) * 3) + 2) + (18 * lux)] * 2.55F;
      }
      difference_between_targets = target[lux] - previous_target[lux];
      percentage_in_the_period = par[((phase / 2) * 3) + 1 + (18 * lux)]; // duration
      percentage_in_the_period = (hourRTC - par[((phase / 2) * 3) + (18 * lux)]) / percentage_in_the_period; // minutes passed / duration
      instant_target[lux] = previous_target[lux] + (difference_between_targets * percentage_in_the_period);
      if (difference_between_targets < 0)
      {
        difference_between_targets = 0 - difference_between_targets;
      }
      else if (difference_between_targets == 0)
      {
        difference_between_targets = 1;
      }
      if (par[16] > 0) // if duration > 0 will be removed 2,5 seconds
      {
        speed_to_target[lux] = ((par[(((phase / 2) * 3) + 1) + (18 * lux)] * 60 * 1000) - 2500) / difference_between_targets;
      }
      else
      {
        speed_to_target[lux] = 40; // 10,2 seconds from 255 to 0
      }
    }
  }
}

void pwm_managing()
{
  if (immediate_preset == 1)
  {
    immediate_preset = 0;
    for (byte i = 0; i < 4; i++)
    {
      pwm_light[i] = instant_target[i];
    }
  }
  else
  {
    unsigned long currentMillis = millis();
    if (currentMillis - prev_millis_minute > 60000)
    {
      if (rtc_not_found == 1)
      {
        hourRTC = hourRTC + 1;
        if (hourRTC == 1440) hourRTC = 0;
      }
      prev_millis_minute = currentMillis;
    }

    if (currentMillis - prev_millis_second > 1000)
    {
      prev_millis_second = currentMillis;
      if (!rtc_not_found) hourRTC = rtc.getHours() * 60;
      if (!rtc_not_found) hourRTC += rtc.getMinutes();
      if ((connection_timeout == 1) || (rtc_not_found == 1))
      {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      }
    }    

    for (byte i = 0; i < 4; i++)
    {
      if (currentMillis - prev_millis_light[i] > speed_to_target[i])
      {
        prev_millis_light[i] = currentMillis;
        if (pwm_light[i] < target[i])
        {
          pwm_light[i] += 1; 
        }
        if (pwm_light[i] > target[i])
        {
          pwm_light[i] -= 1; 
        }
      }
      if ((night_lighting[i] == 1) && (pwm_light[i] < 2))
      {
        pwm_light[i] = 2;
      }
    }
  }
  if (mode_selected == 1)
  {
    analogWrite(PIN_LED_1, 0);
    analogWrite(PIN_LED_2, 0);
    analogWrite(PIN_LED_3, 0);
    analogWrite(PIN_LED_4, 0);
  }
  else if (mode_selected == 2)
  {
    analogWrite(PIN_LED_1, 255);
    analogWrite(PIN_LED_2, 255);
    analogWrite(PIN_LED_3, 255);
    analogWrite(PIN_LED_4, 255);
  }
  else
  {
    analogWrite(PIN_LED_1, 255 - pwm_light[0]);
    analogWrite(PIN_LED_2, 255 - pwm_light[1]);
    analogWrite(PIN_LED_3, 255 - pwm_light[2]);
    analogWrite(PIN_LED_4, 255 - pwm_light[3]);
  }
}

void preset()
{
  par[0]  = 780;  // start hour   phase 1 light 1
  par[1]  = 15;   // duration     phase 1 light 1
  par[2]  = 10;   // target value phase 1 light 1
  par[3]  = 795;  // start hour   phase 2 light 1
  par[4]  = 30;   // duration     phase 2 light 1
  par[5]  = 80;   // target value phase 2 light 1
  par[6]  = 825;  // start hour   phase 3 light 1
  par[7]  = 15;   // duration     phase 3 light 1
  par[8]  = 100;  // target value phase 3 light 1
  par[9]  = 1380; // start hour   phase 4 light 1
  par[10] = 15;   // duration     phase 4 light 1
  par[11] = 80;   // target value phase 4 light 1
  par[12] = 1395; // start hour   phase 5 light 1
  par[13] = 30;   // duration     phase 5 light 1
  par[14] = 10;   // target value phase 5 light 1
  par[15] = 1425; // start hour   phase 6 light 1
  par[16] = 15;   // duration     phase 6 light 1
  par[17] = 0;    // target value phase 6 light 1
  par[18] = 795;  // start hour   phase 1 light 2
  par[19] = 15;   // duration     phase 1 light 2
  par[20] = 10;   // target value phase 1 light 2
  par[21] = 810;  // start hour   phase 2 light 2
  par[22] = 30;   // duration     phase 2 light 2
  par[23] = 80;   // target value phase 2 light 2
  par[24] = 840;  // start hour   phase 3 light 2
  par[25] = 15;   // duration     phase 3 light 2
  par[26] = 100;  // target value phase 3 light 2
  par[27] = 1365; // start hour   phase 4 light 2
  par[28] = 15;   // duration     phase 4 light 2
  par[29] = 80;   // target value phase 4 light 2
  par[30] = 1380; // start hour   phase 5 light 2
  par[31] = 30;   // duration     phase 5 light 2
  par[32] = 10;   // target value phase 5 light 2
  par[33] = 1410; // start hour   phase 6 light 2
  par[34] = 15;   // duration     phase 6 light 2
  par[35] = 0;    // target value phase 6 light 2
  par[36] = 790;  // start hour   phase 1 light 3
  par[37] = 15;   // duration     phase 1 light 3
  par[38] = 10;   // target value phase 1 light 3
  par[39] = 805;  // start hour   phase 2 light 3
  par[40] = 30;   // duration     phase 2 light 3
  par[41] = 80;   // target value phase 2 light 3
  par[42] = 835;  // start hour   phase 3 light 3
  par[43] = 15;   // duration     phase 3 light 3
  par[44] = 100;  // target value phase 3 light 3
  par[45] = 1370; // start hour   phase 4 light 3
  par[46] = 15;   // duration     phase 4 light 3
  par[47] = 80;   // target value phase 4 light 3
  par[48] = 1385; // start hour   phase 5 light 3
  par[49] = 30;   // duration     phase 5 light 3
  par[50] = 10;   // target value phase 5 light 3
  par[51] = 1415; // start hour   phase 6 light 3
  par[52] = 15;   // duration     phase 6 light 3
  par[53] = 0;    // target value phase 6 light 3
  par[54] = 805;  // start hour   phase 1 light 3
  par[55] = 15;   // duration     phase 1 light 3
  par[56] = 10;   // target value phase 1 light 3
  par[57] = 820;  // start hour   phase 2 light 3
  par[58] = 30;   // duration     phase 2 light 3
  par[59] = 80;   // target value phase 2 light 3
  par[60] = 850;  // start hour   phase 3 light 3
  par[61] = 15;   // duration     phase 3 light 3
  par[62] = 100;  // target value phase 3 light 3
  par[63] = 1355; // start hour   phase 4 light 3
  par[64] = 15;   // duration     phase 4 light 3
  par[65] = 80;   // target value phase 4 light 3
  par[66] = 1370; // start hour   phase 5 light 3
  par[67] = 30;   // duration     phase 5 light 3
  par[68] = 10;   // target value phase 5 light 3
  par[69] = 1400; // start hour   phase 6 light 3
  par[70] = 15;   // duration     phase 6 light 3
  par[71] = 0;    // target value phase 6 light 3
  night_lighting[0] = 0; // Light 1 night_lighting
  night_lighting[1] = 0; // Light 2 night_lighting
  night_lighting[2] = 0; // Light 3 night_lighting
  night_lighting[3] = 0; // Light 3 night_lighting
  name_light[0] = 12; // Light 1 name
  name_light[1] = 17; // Light 2 name
  name_light[2] = 0;  // Light 3 name
  name_light[3] = 5;  // Light 4 name
  save();
}

void save()
{
  EEPROM.write(0, par[0]/60);   // start hour h phase 1 light 1
  EEPROM.write(1, par[0]%60);   // start hour m phase 1 light 1
  EEPROM.write(2, par[1]);      // duration     phase 1 light 1
  EEPROM.write(3, par[2]);      // target value phase 1 light 1
  EEPROM.write(4, par[3]/60);   // start hour h phase 2 light 1
  EEPROM.write(5, par[3]%60);   // start hour m phase 2 light 1
  EEPROM.write(6, par[4]);      // duration     phase 2 light 1
  EEPROM.write(7, par[5]);      // target value phase 2 light 1
  EEPROM.write(8, par[6]/60);   // start hour h phase 3 light 1
  EEPROM.write(9, par[6]%60);   // start hour m phase 3 light 1
  EEPROM.write(10, par[7]);     // duration     phase 3 light 1
  EEPROM.write(11, par[8]);     // target value phase 3 light 1
  EEPROM.write(12, par[9]/60);  // start hour h phase 4 light 1
  EEPROM.write(13, par[9]%60);  // start hour m phase 4 light 1
  EEPROM.write(14, par[10]);    // duration     phase 4 light 1
  EEPROM.write(15, par[11]);    // target value phase 4 light 1
  EEPROM.write(16, par[12]/60); // start hour h phase 5 light 1
  EEPROM.write(17, par[12]%60); // start hour m phase 5 light 1
  EEPROM.write(18, par[13]);    // duration     phase 5 light 1
  EEPROM.write(19, par[14]);    // target value phase 5 light 1
  EEPROM.write(20, par[15]/60); // start hour h phase 6 light 1
  EEPROM.write(21, par[15]%60); // start hour m phase 6 light 1
  EEPROM.write(22, par[16]);    // duration     phase 6 light 1
  EEPROM.write(23, par[17]);    // target value phase 6 light 1
  EEPROM.write(24, par[18]/60); // start hour h phase 1 light 2
  EEPROM.write(25, par[18]%60); // start hour m phase 1 light 2
  EEPROM.write(26, par[19]);    // duration     phase 1 light 2
  EEPROM.write(27, par[20]);    // target value phase 1 light 2
  EEPROM.write(28, par[21]/60); // start hour h phase 2 light 2
  EEPROM.write(29, par[21]%60); // start hour m phase 2 light 2
  EEPROM.write(30, par[22]);    // duration     phase 2 light 2
  EEPROM.write(31, par[23]);    // target value phase 2 light 2
  EEPROM.write(32, par[24]/60); // start hour h phase 3 light 2
  EEPROM.write(33, par[24]%60); // start hour m phase 3 light 2
  EEPROM.write(34, par[25]);    // duration     phase 3 light 2
  EEPROM.write(35, par[26]);    // target value phase 3 light 2
  EEPROM.write(36, par[27]/60); // start hour h phase 4 light 2
  EEPROM.write(37, par[27]%60); // start hour m phase 4 light 2
  EEPROM.write(38, par[28]);    // duration     phase 4 light 2
  EEPROM.write(39, par[29]);    // target value phase 4 light 2
  EEPROM.write(40, par[30]/60); // start hour h phase 5 light 2
  EEPROM.write(41, par[30]%60); // start hour m phase 5 light 2
  EEPROM.write(42, par[31]);    // duration     phase 5 light 2
  EEPROM.write(43, par[32]);    // target value phase 5 light 2
  EEPROM.write(44, par[33]/60); // start hour h phase 6 light 2
  EEPROM.write(45, par[33]%60); // start hour m phase 6 light 2
  EEPROM.write(46, par[34]);    // duration     phase 6 light 2
  EEPROM.write(47, par[35]);    // target value phase 6 light 2
  EEPROM.write(48, par[36]/60); // start hour h phase 1 light 3
  EEPROM.write(49, par[36]%60); // start hour m phase 1 light 3
  EEPROM.write(50, par[37]);    // duration     phase 1 light 3
  EEPROM.write(51, par[38]);    // target value phase 1 light 3
  EEPROM.write(52, par[39]/60); // start hour h phase 2 light 3
  EEPROM.write(53, par[39]%60); // start hour m phase 2 light 3
  EEPROM.write(54, par[40]);    // duration     phase 2 light 3
  EEPROM.write(55, par[41]);    // target value phase 2 light 3
  EEPROM.write(56, par[42]/60); // start hour h phase 3 light 3
  EEPROM.write(57, par[42]%60); // start hour m phase 3 light 3
  EEPROM.write(58, par[43]);    // duration     phase 3 light 3
  EEPROM.write(59, par[44]);    // target value phase 3 light 3
  EEPROM.write(60, par[45]/60); // start hour h phase 4 light 3
  EEPROM.write(61, par[45]%60); // start hour m phase 4 light 3
  EEPROM.write(62, par[46]);    // duration     phase 4 light 3
  EEPROM.write(63, par[47]);    // target value phase 4 light 3
  EEPROM.write(64, par[48]/60); // start hour h phase 5 light 3
  EEPROM.write(65, par[48]%60); // start hour m phase 5 light 3
  EEPROM.write(66, par[49]);    // duration     phase 5 light 3
  EEPROM.write(67, par[50]);    // target value phase 5 light 3
  EEPROM.write(68, par[51]/60); // start hour h phase 6 light 3
  EEPROM.write(69, par[51]%60); // start hour m phase 6 light 3
  EEPROM.write(70, par[52]);    // duration     phase 6 light 3
  EEPROM.write(71, par[53]);    // target value phase 6 light 3
  EEPROM.write(72, par[54]/60); // start hour h phase 1 light 3
  EEPROM.write(73, par[54]%60); // start hour m phase 1 light 3
  EEPROM.write(74, par[55]);    // duration     phase 1 light 3
  EEPROM.write(75, par[56]);    // target value phase 1 light 3
  EEPROM.write(76, par[57]/60); // start hour h phase 2 light 3
  EEPROM.write(77, par[57]%60); // start hour m phase 2 light 3
  EEPROM.write(78, par[58]);    // duration     phase 2 light 3
  EEPROM.write(79, par[59]);    // target value phase 2 light 3
  EEPROM.write(80, par[60]/60); // start hour h phase 3 light 3
  EEPROM.write(81, par[60]%60); // start hour m phase 3 light 3
  EEPROM.write(82, par[61]);    // duration     phase 3 light 3
  EEPROM.write(83, par[62]);    // target value phase 3 light 3
  EEPROM.write(84, par[63]/60); // start hour h phase 4 light 3
  EEPROM.write(85, par[63]%60); // start hour m phase 4 light 3
  EEPROM.write(86, par[64]);    // duration     phase 4 light 3
  EEPROM.write(87, par[65]);    // target value phase 4 light 3
  EEPROM.write(88, par[66]/60); // start hour h phase 5 light 3
  EEPROM.write(89, par[66]%60); // start hour m phase 5 light 3
  EEPROM.write(90, par[67]);    // duration     phase 5 light 3
  EEPROM.write(91, par[68]);    // target value phase 5 light 3
  EEPROM.write(92, par[69]/60); // start hour h phase 6 light 3
  EEPROM.write(93, par[69]%60); // start hour m phase 6 light 3
  EEPROM.write(94, par[70]);    // duration     phase 6 light 3
  EEPROM.write(95, par[71]);    // target value phase 6 light 3
  EEPROM.write(96, night_lighting[0]); // Light 1 night_lighting
  EEPROM.write(97, night_lighting[1]); // Light 2 night_lighting
  EEPROM.write(98, night_lighting[2]); // Light 3 night_lighting
  EEPROM.write(99, night_lighting[3]); // Light 3 night_lighting
  EEPROM.write(100, name_light[0]); // Light 1 name
  EEPROM.write(101, name_light[1]); // Light 2 name
  EEPROM.write(102, name_light[2]); // Light 3 name
  EEPROM.write(103, name_light[3]); // Light 4 name
  EEPROM.commit();
}

void read_eeprom()
{
  par[0]  = EEPROM.read(1) + (60 * EEPROM.read(0));   // start hour   phase 1 light 1
  par[1]  = EEPROM.read(2);                           // duration     phase 1 light 1
  par[2]  = EEPROM.read(3);                           // target value phase 1 light 1
  par[3]  = EEPROM.read(5) + (60 * EEPROM.read(4));   // start hour   phase 2 light 1
  par[4]  = EEPROM.read(6);                           // duration     phase 2 light 1
  par[5]  = EEPROM.read(7);                           // target value phase 2 light 1
  par[6]  = EEPROM.read(9) + (60 * EEPROM.read(8));   // start hour   phase 3 light 1
  par[7]  = EEPROM.read(10);                          // duration     phase 3 light 1
  par[8]  = EEPROM.read(11);                          // target value phase 3 light 1
  par[9]  = EEPROM.read(13) + (60 * EEPROM.read(12)); // start hour   phase 4 light 1
  par[10] = EEPROM.read(14);                          // duration     phase 4 light 1
  par[11] = EEPROM.read(15);                          // target value phase 4 light 1
  par[12] = EEPROM.read(17) + (60 * EEPROM.read(16)); // start hour   phase 5 light 1
  par[13] = EEPROM.read(18);                          // duration     phase 5 light 1
  par[14] = EEPROM.read(19);                          // target value phase 5 light 1
  par[15] = EEPROM.read(21) + (60 * EEPROM.read(20)); // start hour   phase 6 light 1
  par[16] = EEPROM.read(22);                          // duration     phase 6 light 1
  par[17] = EEPROM.read(23);                          // target value phase 6 light 1
  par[18] = EEPROM.read(25) + (60 * EEPROM.read(24)); // start hour   phase 1 light 2
  par[19] = EEPROM.read(26);                          // duration     phase 1 light 2
  par[20] = EEPROM.read(27);                          // target value phase 1 light 2
  par[21] = EEPROM.read(29) + (60 * EEPROM.read(28)); // start hour   phase 2 light 2
  par[22] = EEPROM.read(30);                          // duration     phase 2 light 2
  par[23] = EEPROM.read(31);                          // target value phase 2 light 2
  par[24] = EEPROM.read(33) + (60 * EEPROM.read(32)); // start hour   phase 3 light 2
  par[25] = EEPROM.read(34);                          // duration     phase 3 light 2
  par[26] = EEPROM.read(35);                          // target value phase 3 light 2
  par[27] = EEPROM.read(37) + (60 * EEPROM.read(36)); // start hour   phase 4 light 2
  par[28] = EEPROM.read(38);                          // duration     phase 4 light 2
  par[29] = EEPROM.read(39);                          // target value phase 4 light 2
  par[30] = EEPROM.read(41) + (60 * EEPROM.read(40)); // start hour   phase 5 light 2
  par[31] = EEPROM.read(42);                          // duration     phase 5 light 2
  par[32] = EEPROM.read(43);                          // target value phase 5 light 2
  par[33] = EEPROM.read(45) + (60 * EEPROM.read(44)); // start hour   phase 6 light 2
  par[34] = EEPROM.read(46);                          // duration     phase 6 light 2
  par[35] = EEPROM.read(47);                          // target value phase 6 light 2
  par[36] = EEPROM.read(49) + (60 * EEPROM.read(48)); // start hour   phase 1 light 3
  par[37] = EEPROM.read(50);                          // duration     phase 1 light 3
  par[38] = EEPROM.read(51);                          // target value phase 1 light 3
  par[39] = EEPROM.read(53) + (60 * EEPROM.read(52)); // start hour   phase 2 light 3
  par[40] = EEPROM.read(54);                          // duration     phase 2 light 3
  par[41] = EEPROM.read(55);                          // target value phase 2 light 3
  par[42] = EEPROM.read(57) + (60 * EEPROM.read(56)); // start hour   phase 3 light 3
  par[43] = EEPROM.read(58);                          // duration     phase 3 light 3
  par[44] = EEPROM.read(59);                          // target value phase 3 light 3
  par[45] = EEPROM.read(61) + (60 * EEPROM.read(60)); // start hour   phase 4 light 3
  par[46] = EEPROM.read(62);                          // duration     phase 4 light 3
  par[47] = EEPROM.read(63);                          // target value phase 4 light 3
  par[48] = EEPROM.read(65) + (60 * EEPROM.read(64)); // start hour   phase 5 light 3
  par[49] = EEPROM.read(66);                          // duration     phase 5 light 3
  par[50] = EEPROM.read(67);                          // target value phase 5 light 3
  par[51] = EEPROM.read(69) + (60 * EEPROM.read(68)); // start hour   phase 6 light 3
  par[52] = EEPROM.read(70);                          // duration     phase 6 light 3
  par[53] = EEPROM.read(71);                          // target value phase 6 light 3
  par[54] = EEPROM.read(73) + (60 * EEPROM.read(72)); // start hour   phase 1 light 3
  par[55] = EEPROM.read(74);                          // duration     phase 1 light 3
  par[56] = EEPROM.read(75);                          // target value phase 1 light 3
  par[57] = EEPROM.read(77) + (60 * EEPROM.read(76)); // start hour   phase 2 light 3
  par[58] = EEPROM.read(78);                          // duration     phase 2 light 3
  par[59] = EEPROM.read(79);                          // target value phase 2 light 3
  par[60] = EEPROM.read(81) + (60 * EEPROM.read(80)); // start hour   phase 3 light 3
  par[61] = EEPROM.read(82);                          // duration     phase 3 light 3
  par[62] = EEPROM.read(83);                          // target value phase 3 light 3
  par[63] = EEPROM.read(85) + (60 * EEPROM.read(84)); // start hour   phase 4 light 3
  par[64] = EEPROM.read(86);                          // duration     phase 4 light 3
  par[65] = EEPROM.read(87);                          // target value phase 4 light 3
  par[66] = EEPROM.read(89) + (60 * EEPROM.read(88)); // start hour   phase 5 light 3
  par[67] = EEPROM.read(90);                          // duration     phase 5 light 3
  par[68] = EEPROM.read(91);                          // target value phase 5 light 3
  par[69] = EEPROM.read(93) + (60 * EEPROM.read(92)); // start hour   phase 6 light 3
  par[70] = EEPROM.read(94);                          // duration     phase 6 light 3
  par[71] = EEPROM.read(95);                          // target value phase 6 light 3
  night_lighting[0] = EEPROM.read(96);                // Light 1 night_lighting
  night_lighting[1] = EEPROM.read(97);                // Light 2 night_lighting
  night_lighting[2] = EEPROM.read(98);                // Light 3 night_lighting
  night_lighting[3] = EEPROM.read(99);                // Light 3 night_lighting
  name_light[0] = EEPROM.read(100);                   // Light 1 name
  name_light[1] = EEPROM.read(101);                   // Light 2 name
  name_light[2] = EEPROM.read(102);                   // Light 3 name
  name_light[3] = EEPROM.read(103);                   // Light 4 name
}
