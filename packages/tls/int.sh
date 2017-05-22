if [ $# = 0 ];then
    echo "arg1 num in pipe, arg2 num of pipe"
    exit
fi

function inittable {
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "drop database if exists tpcc1000"
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "create database tpcc1000"
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "use tpcc1000; drop table if exists student"
    len=$[$1+16]
    #not supported
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "use tpcc1000; create table if not exists student2(name integer)"
}


#generate load.sql

head='INSERT INTO student2 VALUES '

cur=1234567

multi=""
function getMultipleFields {
    num=$1
    multi=\($cur\)
    for((i=1;i<$num;i++))do
        multi=${multi}\,\($cur\)
    done
}
getMultipleFields $1

rm -rf load.sql


echo "start to get multiple inserts!!!!"

for((i=0;i<$2;i++))do
    echo $head$multi\; >> load.sql
done

inittable

mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < load.sql

mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 --hex-blob --compact tpcc1000 > back.sql


