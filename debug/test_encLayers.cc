#include "parser/sql_utils.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
#include "util/constants.hh"
#include "util/timer.hh"
#include "util/log.hh"
#include "util/onions.hh"

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;
static std::string embeddedDir="/t/cryt/shadow";

static std::map<std::string, WrapperState*> clients;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;


/*for each field, convert the format to FieldMeta_Wrapper*/


static void init(){
    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    SharedProxyState *shared_ps =
                        new SharedProxyState(ci, embeddedDir , master_key,
                                            determineSecurityRating());
    assert(0 == mysql_thread_init());
    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();
    //Connect end!!
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);
}

static
Item *
encrypt_item_layers_raw(const Item &i, uint64_t IV, std::vector<std::unique_ptr<EncLayer> > &enc_layers) {
    const Item *enc = &i;
    Item *new_enc = NULL;
    for (const auto &it : enc_layers) {
        new_enc = it->encrypt(*enc, IV);
        assert(new_enc);
        enc = new_enc;
    }
    assert(new_enc && new_enc != &i);
    return new_enc;
}


static Item *
decrypt_item_layers_raw(const Item &i, uint64_t IV, std::vector<std::unique_ptr<EncLayer> > &enc_layers) {
    const Item *dec = &i;
    Item *out_i = NULL;
    for (auto it = enc_layers.rbegin(); it != enc_layers.rend(); ++it) {
        out_i = (*it)->decrypt(*dec, IV);
        assert(out_i);
        dec = out_i;
    }
    assert(out_i && out_i != &i);
    return out_i;
}


int
main() {
    init();
    create_embedded_thd(0);

    Create_field *f = new Create_field;
    f->sql_type = MYSQL_TYPE_LONG;

    std::unique_ptr<EncLayer> el(EncLayerFactory::encLayer(oDET,SECLEVEL::RND,*f,"HEHE"));

    auto levels = CURRENT_NUM_LAYOUT[oDET];
    std::vector<std::unique_ptr<EncLayer> > layers;
    const Create_field * newcf = f;
    onion o = oDET;
    for (auto l: levels) {
        const std::string key = "plainkey";
        std::unique_ptr<EncLayer>
            el(EncLayerFactory::encLayer(o, l, *newcf, key));
        const Create_field &oldcf = *newcf;
        newcf = el->newCreateField(oldcf);
        layers.push_back(std::move(el));
    }
    Item * in = NULL;
    encrypt_item_layers_raw(*in,0,layers);
    decrypt_item_layers_raw(*in,0,layers);
    (void)el;
    (void)f;

    //Then we should encrypt and decrypt.


    return 0;
}

//main/schema.cc:83 is use to create layers of encryption
