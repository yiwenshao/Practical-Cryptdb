#ifndef UTILITIES_H_INCLUDED
#define UTILITIES_H_INCLUDED
#include <string>

//With color in cpp
extern const std::string BOLD_BEGIN;
extern const std::string RED_BEGIN;
extern const std::string GREEN_BEGIN;
extern const std::string COLOR_END;


//get current time in the form year, month, day
struct current_time{
    int year;
    int month;
    int day;
    void get_time();
    void show_time();
};



#endif // UTILITIES_H_INCLUDED
