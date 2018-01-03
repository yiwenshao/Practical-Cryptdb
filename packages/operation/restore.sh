path="Insert"

if [ $# = 1 ];then
    path=$1
fi

files=`find  $path -type f`
arrayfiles=($files)
for data in ${arrayfiles[@]}
do
    ss="mysql -uroot -pletmein -h127.0.0.1 -P3306 tpcc1000 < $data"
    eval $ss
    echo $ss
done

