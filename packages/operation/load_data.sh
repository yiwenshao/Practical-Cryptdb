mysql -P3306 -e "set @@GLOBAL.sql_mode = ''"
cd ../tls/tpcc-mysql/;./my_load.sh 1
