files=`ls| grep '\.sql$'`
arrayfiles=($files)
#IFS=' ' read -r -a arrayfiles <<< "$files"


for data in ${arrayfiles[@]}
do
    ss="mysql -uroot -pletmein -h127.0.0.1 tpcc1000 < $data"
    eval $ss
done

