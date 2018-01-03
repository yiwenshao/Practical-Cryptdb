files=`find  ~/Insert/ -type f`
arrayfiles=($files)
#IFS=' ' read -r -a arrayfiles <<< "$files"

for data in ${arrayfiles[@]}
do
    ss="cdb_test tpcc1000 < $data"
    eval $ss
    echo $ss
done

