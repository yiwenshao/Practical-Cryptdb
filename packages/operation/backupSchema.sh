db='tpcc1000'
if [ $# = 1 ];then
    db=$1
fi

dir=${db}_schema
rm -rf $dir
mkdir $dir
../tls/mysql_wrapper/backupSchema $db > ${dir}/schema
cp -r ../../shadow ${dir}

