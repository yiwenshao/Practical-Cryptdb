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
    ss="./obj/main/cdb_test ${target_db} < $data"
    eval $ss
    echo $ss
done

