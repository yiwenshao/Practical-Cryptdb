mysql -uroot -pletmein -h127.0.0.1 -P3306 -e "set @@GLOBAL.sql_mode = ''"
cd ../tls/tpcc-mysql/;./my_load.sh 1
