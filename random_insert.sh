str_len=16
function generate_insert_int(){
    head=$1
    pipe=$2
    count=$3
    for((i=1;i<$count;i++))do
        res=$head
        for((j=1;j<$pipe;j++))do
           cur=`./mtl/rand_str $str_len`
           res="${res}('$cur'),"
        done
        cur=`./mtl/rand_str $str_len`
        res="${res}('$cur');"
        echo $res
    done

}

h="INSERT INTO str_table VALUES "
generate_insert_int "$h" 3 100

