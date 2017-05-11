mysqladmin create tpcc1000 -uroot -pletmein -h127.0.0.1
mysql -uroot -pletmein -h127.0.0.1 tpcc1000 < create_table.sql
./tpcc_load -h127.0.0.1 -uroot -pletmein -d tpcc1000 -w 2
mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 tpcc1000 > all.sql
