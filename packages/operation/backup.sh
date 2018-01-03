name="all.sql"

if [ $# = 1 ];then
    name=$1
fi
mysqldump -uroot -h127.0.0.1 -P3306 --no-create-info --compact tpcc1000 > $name

