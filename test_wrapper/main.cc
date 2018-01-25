#include "wrapper/reuse.hh"
#include <string>
#include <iostream>
#include <vector>

using std::string;
using std::vector;

int main() {
    vector<string> inputstr{string("a\n\n\0",4),"b","c"};
    writeColumndataEscapeString(inputstr,"datastr",10);

    vector<string> resstr;
    loadFileEscape("datastr",resstr,10);

    vector<string> inputint{"123","234","345","456","567","678"};

    writeColumndataNum(inputint,"dataint");

    vector<string> resint;
    loadFileNoEscape("dataint",resint);

    return 0;
}
