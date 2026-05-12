#include "GpsInterface.h"
#include "CommandLine.h"
#include "settings.h"
#include <time.h>
extern Settings settings_obj;
extern CommandLine cli_obj;

#ifdef HAS_GPS

extern GpsInterface gps_obj;

char nmeaBuffer[100];

MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

#define GpsSerial Serial1

void GpsInterface::begin() {
  bool module_found = false;

#if defined(MARAUDER_FLIPPER_C5) || defined(GPS_ON_PIN)
#ifndef MARAUDER_FLIPPER_C5
  Serial.printf("[GPS] Powering ON Module (GPIO %d)...\n", GPS_ON_PIN);
#endif
  pinMode(GPS_ON_PIN, OUTPUT);
  digitalWrite(GPS_ON_PIN, HIGH);
  delay(1000); // 1s for module to stabilize
#endif

  // 1. Auto-Probe at 9600 baud (Standard for ATGM336H/Quectel)
  GpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  unsigned long start = millis();
  while (millis() - start < 1500) {
    if (GpsSerial.available()) {
      if (GpsSerial.read() == '$') {
        module_found = true;
        break;
      }
    }
  }

  // 2. If not found, try 115200 baud (High-speed default)
  if (!module_found) {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Probing 115200 baud..."));
#endif
    GpsSerial.begin(115200, SERIAL_8N1, GPS_RX, GPS_TX);
    start = millis();
    while (millis() - start < 1000) {
      if (GpsSerial.available()) {
        if (GpsSerial.read() == '$') {
          module_found = true;
          break;
        }
      }
    }
  }

  // 3. Result handling
  if (!module_found) {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Module NOT FOUND. Disabling GPS features."));
#endif
#ifdef GPS_ON_PIN
    digitalWrite(GPS_ON_PIN, LOW); // Power down the empty slot/interference
#endif
    this->gps_enabled = false;
    return;
  }

  /*// 4. If found at 9600, upgrade to 115200 for performance
  this->sendPMTKCommand("PMTK251,115200");
  delay(200);
  GpsSerial.begin(115200, SERIAL_8N1, GPS_RX, GPS_TX);*/

// Advanced Configuration Sequence (Minino Style)
#ifndef MARAUDER_FLIPPER_C5
  Serial.println(F("[GPS] Applying Advanced Configuration..."));
#endif

  this->sendPMTKCommand("PMTK101"); // Hot Start
  delay(200);
  this->sendPMTKCommand(
      "PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0"); // All NMEA
  delay(100);

  bool agnss = settings_obj.loadSetting<bool>("GPS_AGNSS");
  bool advanced = settings_obj.loadSetting<bool>("GPS_Advanced");
  uint8_t rate = settings_obj.loadSetting<uint8_t>("GPS_UpdateRate");

  this->setAGNSS(agnss);
  this->setUpdateRate(rate == 0 ? 1 : rate);
  this->setConstellations(advanced);

  this->gps_enabled = true;
  this->type_flag = GPSTYPE_NATIVE;
  this->disable_queue();

  nmea.setUnknownSentenceHandler(gps_nmea_notimp);
}

// passthrough for other objects
void gps_nmea_notimp(MicroNMEA &nmea) { gps_obj.enqueue(nmea); }

void GpsInterface::enqueue(MicroNMEA &nmea) {
  std::string nmea_sentence = std::string(nmea.getSentence());

  if (nmea_sentence.length()) {
    this->notimp_nmea_sentence = nmea_sentence.c_str();

    bool unparsed = 1;
    bool enqueue = 1;

    char system = nmea.getTalkerID();
    String msg_id = nmea.getMessageID();
    int length = nmea_sentence.length();

    if (length > 0 && length < 256) {
      if (system) {
        if (msg_id == "TXT") {
          if (length > 8) {
            std::string content = nmea_sentence.substr(7, std::string::npos);

            int tot_brk = content.find(',');
            int num_brk = content.find(',', tot_brk + 1);
            int txt_brk = content.find(',', num_brk + 1);
            int chk_brk = content.rfind('*');

            if (tot_brk != std::string::npos && num_brk != std::string::npos &&
                txt_brk != std::string::npos && chk_brk != std::string::npos &&
                chk_brk > txt_brk && txt_brk > num_brk && num_brk > tot_brk &&
                tot_brk >= 0) {
              std::string total_str = content.substr(0, tot_brk);
              std::string num_str =
                  content.substr(tot_brk + 1, num_brk - tot_brk - 1);
              std::string type_str =
                  content.substr(num_brk + 1, txt_brk - num_brk - 1);
              std::string text_str =
                  content.substr(txt_brk + 1, chk_brk - txt_brk - 1);
              std::string checksum =
                  content.substr(chk_brk + 1, std::string::npos);

              int total = 0;
              if (total_str.length())
                total = atoi(total_str.c_str());

              int num = 0;
              if (num_str.length())
                num = atoi(num_str.c_str());

              int type = 0;
              if (type_str.length())
                type = atoi(type_str.c_str());

              if (text_str.length() && checksum.length()) {
                String text = text_str.c_str();
                if (type > 1) {
                  char type_cstr[4];
                  snprintf(type_cstr, 4, "%02d ", type);
                  type_cstr[3] = '\0';
                  text = type_cstr + text;
                }

                if ((num <= 1 || total <= 1) && this->queue_enabled_flag) {
                  if (this->text) {
                    if (this->text_in) {
                      int size = text_in->size();
                      if (size) {
#ifdef GPS_TEXT_MAXCYCLES
                        if (this->text_cycles >= GPS_TEXT_MAXCYCLES) {
#else
                        if (this->text_cycles) {
#endif
                          if (this->text->size()) {
                            LinkedList<String> *delme = this->text;
                            this->text = new LinkedList<String>;
                            delete delme;
                            this->text_cycles = 0;
                          }
                        }

                        for (int i = 0; i < size; i++) {
                          this->text->add(this->text_in->get(i));
                        }
                        LinkedList<String> *delme = this->text_in;
                        this->text_in = new LinkedList<String>;
                        delete delme;
                        this->text_cycles++;

                        this->gps_text = text;
                      }
                    } else
                      this->text_in = new LinkedList<String>;
                  } else {
                    if (this->text_in) {
                      this->text_cycles = 0;
                      this->text = this->text_in;
                      if (this->text->size()) {
                        if (this->gps_text == "")
                          this->gps_text = this->text->get(0);
                        this->text_cycles++;
                      }
                      this->text_in = new LinkedList<String>;
                    } else {
                      this->text_cycles = 0;
                      this->text = new LinkedList<String>;
                      this->text_in = new LinkedList<String>;
                    }
                  }

                  this->text_in->add(text);
                } else if (this->queue_enabled_flag) {
                  if (!this->text_in)
                    this->text_in = new LinkedList<String>;
                  this->text_in->add(text);
                  int size = this->text_in->size();

#ifdef GPS_TEXT_MAXLINES
                  if (size >= GPS_TEXT_MAXLINES) {
#else
                  if (size >= 5) {
#endif
#ifdef GPS_TEXT_MAXCYCLES
                    if (this->text_cycles >= GPS_TEXT_MAXCYCLES) {
#else
                    if (this->text_cycles) {
#endif
                      if (this->text->size()) {
                        LinkedList<String> *delme = this->text;
                        this->text = new LinkedList<String>;
                        delete delme;
                        this->text_cycles = 0;
                      }
                    }

                    for (int i = 0; i < size; i++)
                      this->text->add(this->text_in->get(i));

                    LinkedList<String> *delme = this->text_in;
                    this->text_in = new LinkedList<String>;
                    delete delme;
                    this->text_cycles++;
                  }
                } else if (num <= 1 || total <= 1)
                  this->gps_text = text;

                if (this->gps_text == "")
                  this->gps_text = text;
                unparsed = 0;
              }
            }
          }
        }
      }
    }

    if (unparsed)
      this->notparsed_nmea_sentence = nmea_sentence.c_str();

    if (this->queue_enabled_flag) {
      if (enqueue) {
        nmea_sentence_t line = {unparsed, msg_id, nmea_sentence.c_str()};

        if (this->queue) {
#ifdef GPS_NMEA_MAXQUEUE
          if (this->queue->size() >= GPS_NMEA_MAXQUEUE)
#else
          if (this->queue->size() >= 30)
#endif
            this->flush_queue();
        } else
          this->new_queue();

        this->queue->add(line);
      } else if (!this->queue)
        this->new_queue();
    } else
      this->flush_queue();
  } else if (!this->queue_enabled_flag)
    this->flush_queue();
}

void GpsInterface::enable_queue() {
  if (this->queue_enabled_flag) {
    if (!this->queue)
      this->new_queue();
    if (!this->text)
      this->text = new LinkedList<String>;
    if (!this->text_in)
      this->text_in = new LinkedList<String>;
  } else {
    this->flush_queue();
    this->queue_enabled_flag = 1;
  }
}

void GpsInterface::disable_queue() {
  this->queue_enabled_flag = 0;
  this->flush_queue();
}

bool GpsInterface::queue_enabled() { return this->queue_enabled_flag; }

LinkedList<nmea_sentence_t> *GpsInterface::get_queue() { return this->queue; }

void GpsInterface::new_queue() {
  this->queue = new LinkedList<nmea_sentence_t>;
}

void GpsInterface::flush_queue() {
  this->flush_queue_nmea();
  this->flush_text();
}

void GpsInterface::flush_queue_nmea() {
  if (this->queue) {
    if (this->queue->size()) {
      LinkedList<nmea_sentence_t> *delme = this->queue;
      this->new_queue();
      delete delme;
    }
  } else
    this->new_queue();
}

void GpsInterface::flush_text() {
  this->flush_queue_text();
  this->flush_queue_textin();
}

void GpsInterface::flush_queue_text() {
  this->text_cycles = 0;

  if (this->text) {
    if (this->text->size()) {
      LinkedList<String> *delme = this->text;
      this->text = new LinkedList<String>;
      delete delme;
    }
  } else
    this->text = new LinkedList<String>;
}

void GpsInterface::flush_queue_textin() {
  if (this->text_in) {
    if (this->text_in->size()) {
      LinkedList<String> *delme = this->text_in;
      this->text_in = new LinkedList<String>;
      delete delme;
    }
  } else
    this->text_in = new LinkedList<String>;
}

void GpsInterface::sendSentence(const char *sentence) {
  MicroNMEA::sendSentence(GpsSerial, sentence);
}

void GpsInterface::sendSentence(Stream &s, const char *sentence) {
  MicroNMEA::sendSentence(s, sentence);
}

void GpsInterface::sendPMTKCommand(const char *command) {
  // Use MicroNMEA to send the sentence which handles checksum and $/*
  MicroNMEA::sendSentence(GpsSerial, command);
  delay(100);
}

void GpsInterface::setAGNSS(bool enabled) {
  if (enabled) {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Enabling AGNSS"));
#endif
    this->sendPMTKCommand("PMTK869,1");
  } else {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Disabling AGNSS"));
#endif
    this->sendPMTKCommand("PMTK869,0");
  }
}

void GpsInterface::setUpdateRate(uint8_t rate_hz) {
  char cmd[32];
  uint16_t interval = 1000;
  if (rate_hz == 5)
    interval = 200;
  else if (rate_hz == 10)
    interval = 100;
  else
    rate_hz = 1; // Fallback to 1Hz

#ifndef MARAUDER_FLIPPER_C5
  Serial.print(F("[GPS] Setting Update Rate to "));
  Serial.print(rate_hz);
  Serial.println(F("Hz"));
#endif
  snprintf(cmd, sizeof(cmd), "PMTK220,%d", interval);
  this->sendPMTKCommand(cmd);
}

void GpsInterface::setConstellations(bool advanced) {
  if (advanced) {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Enabling Multi-Constellation Support"));
#endif
    // GPS, GLONASS, Galileo, BeiDou, QZSS
    this->sendPMTKCommand("PMTK353,1,1,1,1,1,0,0,0,0");
  } else {
#ifndef MARAUDER_FLIPPER_C5
    Serial.println(F("[GPS] Standard Constellations (GPS Only)"));
#endif
    this->sendPMTKCommand("PMTK353,1,0,0,0,0,0,0,0,0");
  }
}

void GpsInterface::setConfigConstellation(String type) {
#ifndef MARAUDER_FLIPPER_C5
  Serial.println("[GPS] Setting PMTK Constellation to: " + type);
#endif

  if (type == "native" || type == "gps")
    this->sendPMTKCommand("PMTK353,1,0,0,0,0,0,0,0,0");
  else if (type == "glonass")
    this->sendPMTKCommand("PMTK353,0,1,0,0,0,0,0,0,0");
  else if (type == "galileo")
    this->sendPMTKCommand("PMTK353,0,0,1,0,0,0,0,0,0");
  else if (type == "beidou" || type == "beidou_bd")
    this->sendPMTKCommand("PMTK353,0,0,0,0,1,0,0,0,0");
  else if (type == "navic" || type == "qzss")
    this->sendPMTKCommand(
        "PMTK353,1,0,0,0,0,0,0,0,0"); // Fallback for unsupported ones
  else
    this->sendPMTKCommand("PMTK353,1,1,1,1,1,0,0,0,0"); // All
}

void GpsInterface::logPOI(String note) {
#ifdef HAS_SD
  if (!settings_obj.loadSetting<bool>("EnableSD")) {
    Serial.println("[POI] Not logged. SD Card disabled in settings.");
    return;
  }

  File logFile = SD.open("/poi_log.csv", FILE_APPEND);
  if (!logFile) {
    Serial.println("[POI] Failed to open /poi_log.csv");
    return;
  }

  // Format: Timestamp, Latitude, Longitude, Altitude, Note
  String lat_str = this->last_lat;
  String lon_str = this->last_lon;
  String dt_str = this->last_datetime_str;

  if (!this->coords_synced) {
    lat_str = "--";
    lon_str = "--";
  }
  if (!this->time_synced) {
    dt_str = "No_Time";
  }

  String csv_line = dt_str + "," + lat_str + "," + lon_str + "," +
                    String(this->last_alt, 2) + "," + note + "\n";
  logFile.print(csv_line);
  logFile.close();

  Serial.println("[POI] Logged: " + note);
#else
  Serial.println("[POI] Not logged. SD Card unsupported.");
#endif
}

void GpsInterface::setType(String t) {
  if (t == "native")
    this->type_flag = GPSTYPE_NATIVE;
  else if (t == "gps")
    this->type_flag = GPSTYPE_GPS;
  else if (t == "glonass")
    this->type_flag = GPSTYPE_GLONASS;
  else if (t == "galileo")
    this->type_flag = GPSTYPE_GALILEO;
  else if (t == "navic")
    this->type_flag = GPSTYPE_NAVIC;
  else if (t == "qzss")
    this->type_flag = GPSTYPE_QZSS;
  else if (t == "beidou")
    this->type_flag = GPSTYPE_BEIDOU;
  else if (t == "beidou_bd")
    this->type_flag = GPSTYPE_BEIDOU_BD;
  else
    this->type_flag = GPSTYPE_ALL;
}

String GpsInterface::generateGXgga() {
  String msg_type = "$" + this->generateType() + "GGA,";

  char timeStr[11];
  if (this->time_synced) {
    snprintf(timeStr, 11, "%s,", this->last_time_str.c_str());
  } else {
    int h = (int)nmea.getHour();
    int m = (int)nmea.getMinute();
    int s = (int)nmea.getSecond();
    if (h > 23 || m > 59 || s > 59) {
      h = 0;
      m = 0;
      s = 0;
    }
    snprintf(timeStr, 11, "%02d%02d%02d,", h, m, s);
  }

  String latStr, lonStr, latDir, lonDir;
  if (nmea.isValid() || this->coords_synced) {
    long lat = (nmea.isValid()) ? nmea.getLatitude()
                                : (long)(this->last_lat.toFloat() * 1000000);
    latDir = lat < 0 ? 'S' : 'N';
    lat = abs(lat);
    char lBuf[12];
    snprintf(lBuf, 12, "%02ld%08.5f,", lat / 1000000,
             ((lat % 1000000) * 60) / 1000000.0);
    latStr = String(lBuf);

    long lon = (nmea.isValid()) ? nmea.getLongitude()
                                : (long)(this->last_lon.toFloat() * 1000000);
    lonDir = lon < 0 ? 'W' : 'E';
    lon = abs(lon);
    char lnBuf[13];
    snprintf(lnBuf, 13, "%03ld%08.5f,", lon / 1000000,
             ((lon % 1000000) * 60) / 1000000.0);
    lonStr = String(lnBuf);
  } else {
    latStr = "0000.00000,";
    latDir = "N";
    lonStr = "00000.00000,";
    lonDir = "E";
  }

  int fixQuality = (this->coords_synced || nmea.isValid()) ? 1 : 0;
  char fixStr[3];
  snprintf(fixStr, 3, "%01d,", fixQuality);

  int numSatellites = (nmea.getNumSatellites() > 0) ? nmea.getNumSatellites()
                                                    : this->last_sats_viewed;
  char satStr[4];
  snprintf(satStr, 4, "%02d,", numSatellites);

  unsigned long hdop = nmea.getHDOP();
  if (hdop == 0 && fixQuality == 1)
    hdop = 20;
  char hdopStr[13];
  snprintf(hdopStr, 13, "%01.2f,", 2.5 * (((float)(hdop)) / 10));

  long altitude;
  if (!nmea.getAltitude(altitude))
    altitude = (long)(this->last_alt * 1000);
  char altStr[9];
  snprintf(altStr, 9, "%01.1f,", altitude / 1000.0);

  String message = msg_type + String(timeStr) + latStr + latDir + ',' + lonStr +
                   lonDir + ',' + fixStr + satStr + hdopStr + altStr + "M,,M,,";

  return message;
}

String GpsInterface::generateGXrmc() {
  String msg_type = "$" + this->generateType() + "RMC,";

  char timeStr[11];
  if (this->time_synced) {
    snprintf(timeStr, 11, "%s,", this->last_time_str.c_str());
  } else {
    int h = (int)nmea.getHour();
    int m = (int)nmea.getMinute();
    int s = (int)nmea.getSecond();
    if (h > 23 || m > 59 || s > 59) {
      h = 0;
      m = 0;
      s = 0;
    }
    snprintf(timeStr, 11, "%02d%02d%02d,", h, m, s);
  }

  char dateStr[8];
  if (this->time_synced && this->last_datetime_str.length() >= 10) {
    String y = this->last_datetime_str.substring(2, 4);
    String m = this->last_datetime_str.substring(5, 7);
    String d = this->last_datetime_str.substring(8, 10);
    snprintf(dateStr, 8, "%s%s%s,", d.c_str(), m.c_str(), y.c_str());
  } else {
    snprintf(dateStr, 8, "%02d%02d%02d,", (int)(nmea.getDay()),
             (int)(nmea.getMonth()), (int)(nmea.getYear() % 100));
  }

  char status = (this->coords_synced || nmea.isValid()) ? 'A' : 'V';
  char mode = (this->coords_synced || nmea.isValid()) ? 'A' : 'N';

  String latStr, lonStr, latDir, lonDir;
  if (nmea.isValid() || this->coords_synced) {
    long lat = (nmea.isValid()) ? nmea.getLatitude()
                                : (long)(this->last_lat.toFloat() * 1000000);
    latDir = lat < 0 ? 'S' : 'N';
    lat = abs(lat);
    char lBuf[12];
    snprintf(lBuf, 12, "%02ld%08.5f,", lat / 1000000,
             ((lat % 1000000) * 60) / 1000000.0);
    latStr = String(lBuf);

    long lon = (nmea.isValid()) ? nmea.getLongitude()
                                : (long)(this->last_lon.toFloat() * 1000000);
    lonDir = lon < 0 ? 'W' : 'E';
    lon = abs(lon);
    char lnBuf[13];
    snprintf(lnBuf, 13, "%03ld%08.5f,", lon / 1000000,
             ((lon % 1000000) * 60) / 1000000.0);
    lonStr = String(lnBuf);
  } else {
    latStr = "0000.00000,";
    latDir = "N";
    lonStr = "00000.00000,";
    lonDir = "E";
  }

  char speedStr[8];
  snprintf(speedStr, 8, "%01.1f,", nmea.getSpeed() / 1000.0);

  char courseStr[7];
  snprintf(courseStr, 7, "%01.1f,", nmea.getCourse() / 1000.0);

  String message = msg_type + String(timeStr) + status + ',' + latStr + latDir +
                   ',' + lonStr + lonDir + ',' + speedStr + courseStr +
                   dateStr + ',' + ',' + mode;
  return message;
}

String GpsInterface::generateType() {
  String msg_type = "";

  if (this->type_flag == GPSTYPE_NATIVE) { // type_flag=0
    char system = this->nav_system;
    if (system)
      msg_type += system;
    else
      msg_type += 'N';
  } else if (this->type_flag == GPSTYPE_GPS) // type_flag=2
    msg_type = "GP";
  else if (this->type_flag == GPSTYPE_GLONASS) // type_flag=3
    msg_type = "GL";
  else if (this->type_flag == GPSTYPE_GALILEO) // type_flag=4
    msg_type = "GA";
  else if (this->type_flag == GPSTYPE_NAVIC) // type_flag=5
    msg_type = "NI";
  else if (this->type_flag == GPSTYPE_QZSS) // type_flag=6
    msg_type = "GQ";
  else if (this->type_flag == GPSTYPE_BEIDOU) // type_flag=7
    msg_type = "BD";
  else if (this->type_flag == GPSTYPE_BEIDOU_BD) { // type_flag=8
    msg_type = "BD";
  } else {
    msg_type = "GN";
  }

  return msg_type;
}

uint8_t GpsInterface::calculateChecksum(const char *sentence) {
  uint8_t checksum = 0;
  const char *p = sentence;
  if (*p == '$')
    p++;
  while (*p && *p != '*') {
    checksum ^= (uint8_t)*p;
    p++;
  }
  return checksum;
}

// Thanks JosephHewitt
String GpsInterface::dt_string_from_gps() {
  // Always return cache if we have nothing better
  if (nmea.getYear() <= 0)
    return this->last_datetime_str;

  // Return a datetime String using GPS data only.
  String datetime = "";
  if (nmea.getYear() > 0) {
    datetime += nmea.getYear();
    datetime += "-";
    datetime += (nmea.getMonth() < 10 ? "0" : "") + String(nmea.getMonth());
    datetime += "-";
    datetime += (nmea.getDay() < 10 ? "0" : "") + String(nmea.getDay());
    datetime += " ";
    datetime += (nmea.getHour() < 10 ? "0" : "") + String(nmea.getHour());
    datetime += ":";
    datetime += (nmea.getMinute() < 10 ? "0" : "") + String(nmea.getMinute());
    datetime += ":";
    datetime += (nmea.getSecond() < 10 ? "0" : "") + String(nmea.getSecond());
  }
  return datetime;
}

void GpsInterface::setGPSInfo() {
  String nmea_sentence = String(nmea.getSentence());
  if (nmea_sentence != "")
    this->nmea_sentence = nmea_sentence;

  this->good_fix = nmea.isValid();
  this->nav_system = nmea.getNavSystem();

  // Update Satellite Count (Use higher value between "In Use" and "In View"
  // Persistence)
  int in_use = nmea.getNumSatellites();
  this->num_sats = (in_use > 0) ? in_use : this->last_sats_viewed;

  // Update Fix Status (Match UI definition: requires positional lock)
  this->good_fix = (nmea.isValid() || this->coords_synced);

  // Update Coordinates (Use Persistence)
  if (nmea.isValid()) {
    this->last_lat = String(nmea.getLatitude() / 1000000.0, 7);
    this->last_lon = String(nmea.getLongitude() / 1000000.0, 7);
    long alt = 0;
    if (nmea.getAltitude(alt))
      this->last_alt = alt / 1000.0;
    this->coords_synced = true;
  }

  if (this->coords_synced) {
    this->lat = this->last_lat;
    this->lon = this->last_lon;
    this->altf = this->last_alt;
  } else {
    this->lat = "Searching...";
    this->lon = "Searching...";
    this->altf = 0.0;
  }

  // Update DateTime (Use Persistence)
  if (this->time_synced) {
    this->datetime = this->last_datetime_str;
  }

  // Update System Text (Harmless filter for External LNA architecture)
  if (nmea_sentence.indexOf("ANTENNA OPEN") != -1) {
    this->gps_text = "Antenna: OK (External LNA)";
  } else if (nmea_sentence.indexOf("ANTENNA OK") != -1) {
    this->gps_text = "Antenna: OK (Internal)";
  } else if (nmea_sentence.indexOf("ANTENNA SHORT") != -1) {
    this->gps_text = "ERROR: ANTENNA SHORT";
  }

  this->accuracy = 2.5 * ((float)nmea.getHDOP() / 10);
  if (this->accuracy == 0 && this->num_sats == 0)
    this->accuracy = 63.75; // Initial value
  if (this->accuracy > 0 && this->accuracy < 63)
    this->last_accuracy = this->accuracy;

  // nmea.clear();
}

float GpsInterface::getAccuracy() {
  if ((this->accuracy == 0 || this->accuracy >= 63.0) &&
      this->last_accuracy < 63.0)
    return this->last_accuracy;
  return this->accuracy;
}

String GpsInterface::getLat() {
  if (this->lat == "Searching..." && this->coords_synced)
    return this->last_lat;
  return this->lat;
}

String GpsInterface::getLon() {
  if (this->lon == "Searching..." && this->coords_synced)
    return this->last_lon;
  return this->lon;
}

float GpsInterface::getAlt() {
  if (this->altf == 0.0 && this->coords_synced)
    return this->last_alt;
  return this->altf;
}

String GpsInterface::getDatetime() { return this->datetime; }

String GpsInterface::getNumSatsString() { return (String)num_sats; }

int GpsInterface::getSatsInView() { return sats_in_view; }

int GpsInterface::getNumSats() { return num_sats; }

bool GpsInterface::getFixStatus() { return this->good_fix; }

String GpsInterface::getFixStatusAsString() {
  if (this->getFixStatus())
    return "Yes";
  else
    return "No";
}

bool GpsInterface::getGpsModuleStatus() { return this->gps_enabled; }

String GpsInterface::getText() { return this->gps_text; }

int GpsInterface::getTextQueueSize() {
  if (this->queue_enabled_flag) {
    bool exists = 0;
    if (this->text) {
      int size = this->text->size();
      if (size)
        return size;
      exists = 1;
    }
    if (this->text_in) {
      int size = this->text_in->size();
      if (size)
        return size;
      exists = 1;
    }
    if (exists)
      return 0;
    else
      return -2;
  } else
    return -1;
}

String GpsInterface::getTextQueue(bool flush) {
  if (this->queue_enabled_flag) {
    if (this->text) {
      int size = this->text->size();
      if (size) {
        String text;
        for (int i = 0; i < size; i++) {
          String now = this->text_in->get(i);
          if (now != "") {
            if (text != "") {
              text += '\r';
              text += '\n';
            }
            text += now;
          }
        }
        if (flush) {
          LinkedList<String> *delme = this->text;
          this->text_cycles = 0;
          this->text = this->text_in;
          if (!this->text)
            this->text = new LinkedList<String>;
          if (this->text->size())
            this->text_cycles++;
          this->text_in = new LinkedList<String>;
          delete delme;
        }
        return text;
      }
    } else {
      this->text = new LinkedList<String>;
      this->text_cycles = 0;
    }

    if (this->text_in) {
      int size = this->text_in->size();
      if (size) {
        LinkedList<String> *buffer = this->text_in;
        if (flush)
          this->text_in = new LinkedList<String>;
        String text;
        for (int i = 0; i < size; i++) {
          String now = buffer->get(i);
          if (now != "") {
            if (text != "") {
              text += '\r';
              text += '\n';
            }
            text += now;
          }
        }
        if (flush)
          delete buffer;
        return text;
      }
    } else
      this->text_in = new LinkedList<String>;

    return this->gps_text;
  } else
    return this->gps_text;
}

String GpsInterface::getNmea() { return this->nmea_sentence; }

String GpsInterface::getNmeaNotimp() { return this->notimp_nmea_sentence; }

String GpsInterface::getNmeaNotparsed() {
  return this->notparsed_nmea_sentence;
}

#include "WiFiScan.h"
extern WiFiScan wifi_scan_obj;

void GpsInterface::main() {
#ifdef HAS_GPS
  if (GpsSerial.available()) {
    while (GpsSerial.available()) {
      char c = GpsSerial.read();
      if (c == '$') {
        buffer_pos = 0;
      }

      // 1. Feed EVERY character to the parser immediately
      nmea.process(c);

      if (buffer_pos < sizeof(nmea_buffer) - 1) {
        nmea_buffer[buffer_pos++] = c;
        nmea_buffer[buffer_pos] = '\0';

        if (c == '\n' || c == '\r') {
          if (buffer_pos < 6) { // Ignore short/junk
            buffer_pos = 0;
            continue;
          }
          // 2. NMEA Normalization Engine (Neutralize GN, GB, BD)
          if (buffer_pos > 5 && nmea_buffer[0] == '$' &&
              (nmea_buffer[1] == 'G' || nmea_buffer[1] == 'B')) {
            bool modified = false;

            // 2.1 Force GP Talker ID
            if (nmea_buffer[2] != 'P') {
              nmea_buffer[1] = 'G';
              nmea_buffer[2] = 'P';
              modified = true;
            }

            // 2.2 Absolute Checksum recalculation
            char *pStar = strchr(nmea_buffer, '*');
            if (pStar && modified) {
              unsigned char ck = 0;
              for (char *p = nmea_buffer + 1; p < pStar; p++)
                ck ^= (unsigned char)(*p);
              char hex[3];
              sprintf(hex, "%02X", ck);
              pStar[1] = hex[0];
              pStar[2] = hex[1];
            }

            // 4. Structural Repair and Forceful Telemetry Injection for UI
            // Dashboards (OUTPUT ONLY)
            const char *repairable[] = {"GGA,", "RMC,", "ZDA,", "GLL,", "GSV,"};
            bool is_repairable = false;
            for (int r = 0; r < 5; r++)
              if (strstr(nmea_buffer, repairable[r]))
                is_repairable = true;

            // Hijacking logic (Assembles 'clean' sentences for the Dashboard)
            if (is_repairable) {
              int commas = 0;
              char hijacked[256] = {
                  0}; // Increased to 256 to stop stack smashing
              int h_pos = 0;
              bool modified_output = false;

              for (int i = 0; i < (int)strlen(nmea_buffer) && h_pos < 250;
                   i++) {
                hijacked[h_pos++] = nmea_buffer[i];
                if (nmea_buffer[i] == ',') {
                  commas++;
                  bool field_empty =
                      (nmea_buffer[i + 1] == ',' || nmea_buffer[i + 1] == '*' ||
                       nmea_buffer[i + 1] == '\0');

                  // 4.1 Force Time Injection
                  bool is_time_field =
                      (commas == 1 && !strstr(nmea_buffer, "GLL,")) ||
                      (commas == 5 && strstr(nmea_buffer, "GLL,"));
                  if (is_time_field && (this->time_synced || field_empty)) {
                    for (int j = 0;
                         j < (int)this->last_time_str.length() && h_pos < 250;
                         j++)
                      hijacked[h_pos++] = this->last_time_str[j];
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.2 Latitude Injection
                  bool is_lat_f =
                      (commas == 2 && strstr(nmea_buffer, "GGA,")) ||
                      (commas == 3 && strstr(nmea_buffer, "RMC,")) ||
                      (commas == 1 && strstr(nmea_buffer, "GLL,"));
                  if (is_lat_f && (field_empty && this->coords_synced)) {
                    double val = abs(this->last_lat.toDouble());
                    char c_buf[16];
                    snprintf(c_buf, 16, "%02d%08.5f", (int)val,
                             (val - (int)val) * 60.0);
                    for (int j = 0; j < (int)strlen(c_buf) && h_pos < 250; j++)
                      hijacked[h_pos++] = c_buf[j];
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.3 Longitude Injection
                  bool is_lon_f =
                      (commas == 4 && strstr(nmea_buffer, "GGA,")) ||
                      (commas == 5 && strstr(nmea_buffer, "RMC,")) ||
                      (commas == 3 && strstr(nmea_buffer, "GLL,"));
                  if (is_lon_f && (field_empty && this->coords_synced)) {
                    double val = abs(this->last_lon.toDouble());
                    char c_buf[16];
                    snprintf(c_buf, 16, "%03d%08.5f", (int)val,
                             (val - (int)val) * 60.0);
                    for (int j = 0; j < (int)strlen(c_buf) && h_pos < 250; j++)
                      hijacked[h_pos++] = c_buf[j];
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.4 N/S and E/W Orientation Injection
                  bool is_ns_f = (commas == 3 && strstr(nmea_buffer, "GGA,")) ||
                                 (commas == 4 && strstr(nmea_buffer, "RMC,")) ||
                                 (commas == 2 && strstr(nmea_buffer, "GLL,"));
                  bool is_ew_f = (commas == 5 && strstr(nmea_buffer, "GGA,")) ||
                                 (commas == 6 && strstr(nmea_buffer, "RMC,")) ||
                                 (commas == 4 && strstr(nmea_buffer, "GLL,"));

                  if ((is_ns_f || is_ew_f) &&
                      (field_empty && this->coords_synced)) {
                    if (is_ns_f && h_pos < 250)
                      hijacked[h_pos++] =
                          (this->last_lat.toDouble() >= 0) ? 'N' : 'S';
                    if (is_ew_f && h_pos < 250)
                      hijacked[h_pos++] =
                          (this->last_lon.toDouble() >= 0) ? 'E' : 'W';
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.5 Status / Fix Quality
                  bool is_fix_f =
                      (commas == 6 && strstr(nmea_buffer, "GGA,")) ||
                      (commas == 2 && strstr(nmea_buffer, "RMC,")) ||
                      (commas == 6 && strstr(nmea_buffer, "GLL,"));
                  if (is_fix_f && h_pos < 250) {
                    bool good = (this->coords_synced || nmea.isValid());
                    if (strstr(nmea_buffer, "GGA,"))
                      hijacked[h_pos++] = good ? '1' : '0';
                    else if (strstr(nmea_buffer, "RMC,"))
                      hijacked[h_pos++] = good ? 'A' : 'V';
                    else if (strstr(nmea_buffer, "GLL,"))
                      hijacked[h_pos++] = good ? 'A' : 'V';

                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.6 Satellites In View
                  bool is_sat_f =
                      (commas == 7 && strstr(nmea_buffer, "GGA,")) ||
                      (commas == 3 && strstr(nmea_buffer, "GSV,"));
                  if (is_sat_f && h_pos < 248) {
                    int s_view =
                        (this->sats_in_view > (int)nmea.getNumSatellites())
                            ? this->sats_in_view
                            : (int)nmea.getNumSatellites();
                    if (s_view == 0)
                      s_view = this->last_sats_viewed;
                    char s_str[4];
                    snprintf(s_str, 4, "%02d", s_view);
                    hijacked[h_pos++] = s_str[0];
                    hijacked[h_pos++] = s_str[1];
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }

                  // 4.7 Altitude and HDOP (GGA only)
                  if (strstr(nmea_buffer, "GGA,")) {
                    if (commas == 8 &&
                        (field_empty || this->last_accuracy < 10.0)) {
                      char h_buf[10];
                      snprintf(h_buf, 10, "%.1f", this->last_accuracy / 2.5);
                      for (int j = 0; j < (int)strlen(h_buf) && h_pos < 250;
                           j++)
                        hijacked[h_pos++] = h_buf[j];
                      while (nmea_buffer[i + 1] != ',' &&
                             nmea_buffer[i + 1] != '*' &&
                             nmea_buffer[i + 1] != '\0')
                        i++;
                      modified_output = true;
                    }
                    if (commas == 9 && (field_empty && this->coords_synced)) {
                      char a_buf[12];
                      snprintf(a_buf, 12, "%.1f", this->last_alt);
                      for (int j = 0; j < (int)strlen(a_buf) && h_pos < 250;
                           j++)
                        hijacked[h_pos++] = a_buf[j];
                      while (nmea_buffer[i + 1] != ',' &&
                             nmea_buffer[i + 1] != '*' &&
                             nmea_buffer[i + 1] != '\0')
                        i++;
                      modified_output = true;
                    }
                  }

                  // 4.8 Date Injection into RMC / ZDA
                  if (strstr(nmea_buffer, "RMC,") && commas == 9 &&
                      (this->time_synced)) {
                    for (int j = 0;
                         j < (int)this->last_date_str.length() && h_pos < 250;
                         j++)
                      hijacked[h_pos++] = this->last_date_str[j];
                    while (nmea_buffer[i + 1] != ',' &&
                           nmea_buffer[i + 1] != '*' &&
                           nmea_buffer[i + 1] != '\0')
                      i++;
                    modified_output = true;
                  }
                  if (strstr(nmea_buffer, "ZDA,") && (this->time_synced)) {
                    if (commas == 2 && h_pos < 248) {
                      char d_str[4];
                      snprintf(
                          d_str, 4, "%02d",
                          (int)this->last_date_str.substring(0, 2).toInt());
                      hijacked[h_pos++] = d_str[0];
                      hijacked[h_pos++] = d_str[1];
                      while (nmea_buffer[i + 1] != ',' &&
                             nmea_buffer[i + 1] != '*' &&
                             nmea_buffer[i + 1] != '\0')
                        i++;
                      modified_output = true;
                    } else if (commas == 3 && h_pos < 248) {
                      char m_str[4];
                      snprintf(
                          m_str, 4, "%02d",
                          (int)this->last_date_str.substring(2, 4).toInt());
                      hijacked[h_pos++] = m_str[0];
                      hijacked[h_pos++] = m_str[1];
                      while (nmea_buffer[i + 1] != ',' &&
                             nmea_buffer[i + 1] != '*' &&
                             nmea_buffer[i + 1] != '\0')
                        i++;
                      modified_output = true;
                    } else if (commas == 4) {
                      String full_y = this->last_datetime_str.substring(0, 4);
                      for (int j = 0; j < (int)full_y.length() && h_pos < 250;
                           j++)
                        hijacked[h_pos++] = full_y[j];
                      while (nmea_buffer[i + 1] != ',' &&
                             nmea_buffer[i + 1] != '*' &&
                             nmea_buffer[i + 1] != '\0')
                        i++;
                      modified_output = true;
                    }
                  }
                }
              }

              if (modified_output) {
                hijacked[h_pos] = '\0';
                strncpy(nmea_buffer, hijacked, sizeof(nmea_buffer) - 1);
                nmea_buffer[sizeof(nmea_buffer) - 1] = '\0';
                pStar = strchr(nmea_buffer, '*');
                if (pStar) {
                  unsigned char ck = 0;
                  for (char *p = nmea_buffer + 1; p < pStar; p++)
                    ck ^= (unsigned char)(*p);
                  char hex[3];
                  sprintf(hex, "%02X", ck);
                  pStar[1] = hex[0];
                  pStar[2] = hex[1];
                  pStar[3] = '\0';
                }
              }
            }

            // 5. Manual Manual DateTime Extraction (ATGM336H / ZDA Support)
            // This ensures clock updates even if MicroNMEA library returns Year
            // 0
            if (strstr(nmea_buffer, "ZDA,") || strstr(nmea_buffer, "RMC,")) {
              LinkedList<String> fields =
                  cli_obj.parseCommand(nmea_buffer, ",");
              if (strstr(nmea_buffer, "ZDA,") && fields.size() >= 5) {
                String time_f = fields.get(1);  // HHMMSS.SS
                String day_f = fields.get(2);   // DD
                String month_f = fields.get(3); // MM
                String year_f = fields.get(4);  // YYYY
                if (year_f.toInt() > 2000) {
                  this->last_time_str = time_f;
                  this->last_date_str = day_f + month_f + year_f.substring(2);
                  this->last_datetime_str =
                      year_f + "-" + month_f + "-" + day_f + " " +
                      time_f.substring(0, 2) + ":" + time_f.substring(2, 4) +
                      ":" + time_f.substring(4, 6);
                  this->time_synced = true;
                }
              } else if (strstr(nmea_buffer, "RMC,") && fields.size() >= 10) {
                String time_f = fields.get(1); // HHMMSS.SS
                String date_f = fields.get(9); // DDMMYY
                if (date_f.length() == 6 && date_f != "000000") {
                  this->last_time_str = time_f;
                  this->last_date_str = date_f;
                  this->last_datetime_str =
                      "20" + date_f.substring(4, 6) + "-" +
                      date_f.substring(2, 4) + "-" + date_f.substring(0, 2) +
                      " " + time_f.substring(0, 2) + ":" +
                      time_f.substring(2, 4) + ":" + time_f.substring(4, 6);
                  this->time_synced = true;
                }
              }
            }

            // 6. Echo NMEA to Serial (Only during active GPS scan/wardrive)
            if (wifi_scan_obj.currentScanMode == WIFI_SCAN_GPS_NMEA &&
                nmea_buffer[0] == '$') {
              // Block noisy hardware messages
              if (strstr(nmea_buffer, "ANTENNA") ||
                  strstr(nmea_buffer, "OPEN") || strstr(nmea_buffer, "SHORT")) {
                buffer_pos = 0;
                continue;
              }

              const char *dashboard_safe[] = {"GGA", "RMC", "GSV", "GSA",
                                              "GLL", "VTG", "ZDA"};
              bool safe = false;
              for (int s = 0; s < 7; s++)
                if (strstr(nmea_buffer, dashboard_safe[s]))
                  safe = true;

              if (safe) {
                // TALKER ID OVERRIDE: Force output to match user-selected
                // constellation
                if (this->type_flag != GPSTYPE_NATIVE &&
                    this->type_flag != GPSTYPE_ALL) {
                  String custom_id = this->generateType();
                  if (custom_id.length() >= 2) {
                    nmea_buffer[1] = custom_id[0];
                    nmea_buffer[2] = custom_id[1];

                    // Recalculate checksum since we modified the header
                    uint8_t new_cksum = this->calculateChecksum(nmea_buffer);
                    char *ck_ptr = strchr(nmea_buffer, '*');
                    if (ck_ptr) {
                      snprintf(ck_ptr + 1, 3, "%02X", new_cksum);
                    }
                  }
                }
                Serial.println(nmea_buffer);
              }
            }

          } // End of normalization block

          // Sync UI variables (Moved outside normalization to ensure updates
          // even for non-G/B talker IDs)
          this->setGPSInfo();

          buffer_pos = 0;
        } // End of NMEA EOL block
      } else {
        buffer_pos = 0;
      }
    }
  }
#endif
}
#endif
