#include"utilities.h"
#include<time.h>
#include<iostream>
extern const std::string BOLD_BEGIN = "\033[1m";
extern const std::string RED_BEGIN = "\033[1;31m";
extern const std::string GREEN_BEGIN = "\033[1;92m";
extern const std::string COLOR_END = "\033[0m";

void current_time::get_time(){
    const time_t t = time(NULL);
    struct tm* cur_time = localtime(&t);
    year = cur_time->tm_year+1900;
    month = cur_time->tm_mon+1;
    day = cur_time->tm_mday;
}

/*
function to show the time
*/
void current_time::show_time(){
    std::cout<<"current year is: "<<year<<
    "current month is: "<<month<<
    "current date of month is: "<<day<<std::endl;
}
