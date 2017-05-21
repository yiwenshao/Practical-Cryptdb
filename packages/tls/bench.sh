if [ $# = 0 ];then
    echo "arg1: len of field, arg2 num in pipe, arg3 num of pipe"
    exit
fi

function inittable {
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "drop database if exists tpcc1000"
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "create database tpcc1000"
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "use tpcc1000; drop table if exists student"
    len=$[$1+16]
    #not supported
    #mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "create table if not exists tpcc1000.student(name varchar(${len}))"
    mysql -uroot -pletmein -h127.0.0.1 -P3399 -e "use tpcc1000; create table if not exists student(name varchar(${len}))"
    echo "create table if not exists tpcc1000.student(name varchar(${len}))"
}


#generate load.sql

head='INSERT INTO student VALUES '

cur=""

function getField {
    num=$1
    for((i=0;i<$num;i++))do 
        cur=${cur}a
    done
    cur=\'$cur\'
}
getField $1
multi=""
function getMultipleFields {
    num=$1
    multi=\($cur\)
    for((i=1;i<$num;i++))do
        multi=${multi}\,\($cur\)
    done
}
getMultipleFields $2

rm -rf load.sql

for((i=0;i<$3;i++))do
    echo $head$multi\; >> load.sql
done


inittable $1
mysql -uroot -pletmein -h127.0.0.1 -P3399 tpcc1000 < load.sql
mysqldump --skip-extended-insert -uroot -pletmein -h127.0.0.1 --hex-blob --compact tpcc1000 > back.sql


