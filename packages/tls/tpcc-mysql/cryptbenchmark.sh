echo "=============================================CREATE DATABASE====================================================="
mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "create database tpcc1000"
echo "=============================================CREATE TABLE====================================================="
mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < create_table.sql
echo "=============================================LOAD DATA====================================================="
mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < all.sql
echo "=============================================DUMP DATABASE====================================================="
mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 > back.sql
