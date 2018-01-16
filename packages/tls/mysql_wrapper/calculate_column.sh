dir="allTables"

function calculate_column () {
    token=$1
    echo $1:
    res=`find ${dir} -type f | grep "${token}"| xargs du | awk 'NR==1{res=0;}{res+=$1}END{print res}'`
    resM=`expr $res / 1024`
    echo $res KB OR $resM MB
}


calculate_column ADD
calculate_column Eq
calculate_column Order
calculate_column SWP
calculate_column cdb_salt
calculate_column
