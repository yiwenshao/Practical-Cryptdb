# CryptdbModify

Cryptdb originated from MIT. This is a modified version try to add new features and fix bugs we meet in our environment. 
Introduction to the features included will be posted at yiwenshao.github.io.

To deploy this version, you need.

1. compile MySQL5.5 with the following command

```
mkdir build
cd build
export CXX=g++-4.7
cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off ..
make
```
2. set MySQL-SRC in conf/config.mk

3. have g++-4.7 and use make to compile

4. install MySQL-proxy

If you meet any problems installing it, contact me via shaoyiwenetATgmailDotcom.

