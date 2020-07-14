/*
   Based on https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon
*/

uint8_t power = 1;
uint8_t brightness = 128;

//String setPower(String value) {
//  power = value.toInt();
//  if(power < 0) power = 0;
//  else if (power > 1) power = 1;
//  return String(power);
//}

String getPower() {
  return String(power);
}

String getBrightness() {
  return String(brightness);
}

String getHourColor() {
  return String(hourColor);
}

String getMinuteColor() {
  return String(minuteColor);
}

String getSecondColor() {
  return String(secondColor);
}

FieldList fields = {
  { "power", "Power", BooleanFieldType, 0, 1, getPower },
  { "brightness", "Brightness", NumberFieldType, 1, 255, getBrightness },
  { "hour",   "Hour", NumberFieldType, 0, 255, getHourColor },
  { "minute", "Minute", NumberFieldType, 0, 255, getMinuteColor },
  { "second", "Second", NumberFieldType, 1, 255, getSecondColor },
};

uint8_t fieldCount = ARRAY_SIZE(fields);
