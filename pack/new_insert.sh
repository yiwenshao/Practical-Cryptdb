dir="~/Insert/"
cur_dir=`pwd`
cdb_test_dir="../../"
target_db="micro_db"

cd ${cdb_test_dir}

if [ $#=1 ];then
    dir=${cur_dir}/$1
fi

files=`find  ${dir} -type f`
arrayfiles=($files)
for data in ${arrayfiles[@]}
do
    ss="./mtl/batch_insert ${target_db} localhost < $data"
#    eval $ss
    echo $ss
done

