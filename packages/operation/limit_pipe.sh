#backup data with limited pipecount
current_dir=`pwd`

db=tpcc1000
pipe=100
tables=`mysql -uroot -h127.0.0.1 -P3306 $db -e "show tables" | awk '{print $1}'`
table_array=($tables)
len=${#table_array[@]}
back_dir=pipe_$pipe
rm -rf $back_dir
mkdir $back_dir
cd ../tls/mysql_wrapper/
for((i=1;i<$len;i++))
do
    cmd="./main 100 $db ${table_array[$i]}"
    eval $cmd > $current_dir/$back_dir/${table_array[$i]}.sql
done

