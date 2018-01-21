#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>

int
main() {
    AES_KEY *mk = getKey("");
    std::string realkey = getLayerKey(mk,"hehe",SECLEVEL::HOM);
    std::unique_ptr<EncLayer> q(new HOM(realkey));
    (void)q;

    return 0;
}
