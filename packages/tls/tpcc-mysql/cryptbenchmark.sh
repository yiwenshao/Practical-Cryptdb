mysqladmin create tpcc1000 -uroot -pletmein -h127.0.0.1 -P3399
mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < create_table.sql
mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < all.sql
mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 > crypt.sql
