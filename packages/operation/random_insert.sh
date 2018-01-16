function generate_insert_int(){
    head=$1
    pipe=$2
    count=$3
    for((i=1;i<$count;i++))do
        res=$head
        for((j=1;j<$pipe;j++))do
            res="${res}($RANDOM),"
        done
        res="${res}($RANDOM);"
        echo $res
    done

}

h="INSERT INTO int_table VALUES "
generate_insert_int "$h" 10 10
