#include <stdio.h>
#include "./tiny/tinyxml.h"
#include <iostream>   // std::cout
#include <string>     // std::string, std::to_string


char inited = 0;

static TiXmlDocument doc("settings.xml");

float get_value(const char *name)
{
    double  result;
    
    if (inited == 0) {
        doc.LoadFile();
        inited = 1;
    }

    TiXmlElement *element = doc.FirstChildElement(name);
    
    if (element == NULL) {
        printf("incorrect xml name %s\n", name);
        exit(-1);
    }
    element->QueryDoubleAttribute("val", &result);
   
    return result/1000.0;
}

void set_value(const char *name, float value)
{
    if (inited == 0) {
        doc.LoadFile();
        inited = 1;
    }
    
    TiXmlElement *element = doc.FirstChildElement(name);
    
    if (element == NULL) {
        printf("incorrect xml name %s\n", name);
        exit(-1);
    }
    
    element->SetAttribute ("val", 1000.0 * value);
    doc.SaveFile("settings.xml");
}
