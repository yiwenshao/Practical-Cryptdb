#include "main/CryptoHandlers.hh"

static void
test_aes_cmc(int num_of_tests,int len) {
    std::string key(16,'a');
    AES_KEY * ak = get_AES_dec_key(key);
//    std::string plain(len,'a');
    std::string plain = "abc";
    std::string enc,dec;
    for(int i=0;i<num_of_tests;i++){
        enc = encrypt_AES_CMC(plain,ak,true);
    }

    for(int i=0;i<num_of_tests;i++){
        dec = decrypt_AES_CMC(enc,ak,true);
    }
}

static void
test_aes_cbc(int num_of_tests,int len) {


}


int 
main() {
    test_aes_cmc(10,10);
    test_aes_cbc(10,10);
    return 0;
}
