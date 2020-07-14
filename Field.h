/*
   Based on https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon
*/

typedef String (*FieldSetter)(String);
typedef String (*FieldGetter)();


const String NumberFieldType = "Number";
const String BooleanFieldType = "Boolean";
const String SelectFieldType = "Select";
const String ColorFieldType = "Color";
const String SectionFieldType = "Section";

typedef struct Field {
  String name;
  String label;
  String type;
  uint8_t min;
  uint8_t max;
  FieldGetter getValue;
  FieldGetter getOptions;
  FieldSetter setValue;
};

typedef Field FieldList[];

Field getField(String name, FieldList fields, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    Field field = fields[i];
    if (field.name == name) {
      return field;
    }
  }
  return Field();
}

String getFieldValue(String name, FieldList fields, uint8_t count) {
  Field field = getField(name, fields, count);
  if (field.getValue) {
    return field.getValue();
  }
  return String();
}

String setFieldValue(String name, String value, FieldList fields, uint8_t count) {
  Field field = getField(name, fields, count);
  if (field.setValue) {
    return field.setValue(value);
  }
  return String();
}

String getFieldsJson(FieldList fields, uint8_t count) {
  String json = "[";

  for (uint8_t i = 0; i < count; i++) {
    Field field = fields[i];

    json += "{\"name\":\"" + field.name + "\",\"label\":\"" + field.label + "\",\"type\":\"" + field.type + "\"";

    if(field.getValue) {
      if (field.type == ColorFieldType || field.type == "String") {
        json += ",\"value\":\"" + field.getValue() + "\"";
      }
      else {
        json += ",\"value\":" + field.getValue();
      }
    }

    if (field.type == NumberFieldType) {
      json += ",\"min\":" + String(field.min);
      json += ",\"max\":" + String(field.max);
    }

    if (field.getOptions) {
      json += ",\"options\":[";
      json += field.getOptions();
      json += "]";
    }

    json += "}";

    if (i < count - 1)
      json += ",";
  }
  return json;
}

/*
  String json = "[";

  json += "{\"name\":\"power\",\"label\":\"Power\",\"type\":\"Boolean\",\"value\":" + String(power) + "},";
  json += "{\"name\":\"brightness\",\"label\":\"Brightness\",\"type\":\"Number\",\"value\":" + String(brightness) + "},";

  json += "{\"name\":\"pattern\",\"label\":\"Pattern\",\"type\":\"Select\",\"value\":" + String(currentPatternIndex) + ",\"options\":[";
  for (uint8_t i = 0; i < patternCount; i++)
  {
    json += "\"" + patterns[i].name + "\"";
    if (i < patternCount - 1)
      json += ",";
  }
  json += "]},";

  json += "{\"name\":\"autoplay\",\"label\":\"Autoplay\",\"type\":\"Boolean\",\"value\":" + String(autoplay) + "},";
  json += "{\"name\":\"autoplayDuration\",\"label\":\"Autoplay Duration\",\"type\":\"Number\",\"value\":" + String(autoplayDuration) + "},";

  json += "{\"name\":\"solidColor\",\"label\":\"Color\",\"type\":\"Color\",\"value\":\"" + String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b) +"\"},";

  json += "{\"name\":\"cooling\",\"label\":\"Cooling\",\"type\":\"Number\",\"value\":" + String(cooling) + "},";
  json += "{\"name\":\"sparking\",\"label\":\"Sparking\",\"type\":\"Number\",\"value\":" + String(sparking) + "}";

  json += "]";
*/
