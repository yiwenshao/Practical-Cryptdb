mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb2"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb3"
rm -rf ./shadow/*

##running this twice will caurse troubles because of the metadata mismatch
./obj/main/change_test
