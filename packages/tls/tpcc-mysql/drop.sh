mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 -e "create database tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 tpcc1000 < create_table.sql
