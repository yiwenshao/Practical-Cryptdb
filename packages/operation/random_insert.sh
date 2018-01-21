##echo $(rand 0 65535)
function rand(){  
    min=$1  
    max=$(($2-$min+1))  
    num=$(($RANDOM+1000000000)) #增加一个10位的数再求余  
    echo $(($num%$max+$min))  
}




##[0, 32767]
function generate_insert_int(){
    head=$1
    pipe=$2
    count=$3
    for((i=1;i<=$count;i++))do
        res=$head
        for((j=1;j<$pipe;j++))do
            res="${res}($RANDOM),"
        done
        res="${res}($RANDOM);"
        echo $res
    done

}

h="INSERT INTO int_table VALUES "
generate_insert_int "$h" 100 10000
