
# load data and dump all.sql
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 -e "create database tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 tpcc1000 < create_table.sql

./tpcc_load -h127.0.0.1 -uroot -pletmein -d tpcc1000 -w $1
# mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 --no-create-info --hex-blob --compact tpcc1000 > all$1.sql

#mysqldump -uroot -pletmein -h127.0.0.1  --hex-blob --no-create-info --compact tpcc1000 --compact > all$1.sql

#mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
