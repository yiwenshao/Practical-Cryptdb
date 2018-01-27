function get_split(){
    b=$1
    e=$2
    f=$3
    pname=$4
    sed -n "${b},${e}p" $f > ${f}_${pname}
}


if [ $# != 2 ];then
    echo "should give the text name, and the num of splits"
    exit
fi

base=0
filename=$1
total_line=`cat $filename | wc -l`
num_of_splits=$2
echo total_line: $total_line

step=$[ $total_line/$num_of_splits ]
echo step: $step

#get_split 1 5 text 1

cnt=1
for((;$base<$total_line;));do
    echo $[ $base + 1 ] ,  $[ $base + $step ], cnt: $cnt
    get_split $[ $base + 1 ] $[ $base + $step ] $filename $cnt
    base=$[ $base + $step ]
    cnt=$[ $cnt + 1 ]
done
