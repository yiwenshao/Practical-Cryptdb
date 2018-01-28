name="all.sql"

if [ $# = 1 ];then
    name=$1
fi
mysqldump -uroot -hlocalhost -P3306 --no-create-info --compact micro_db > $name

