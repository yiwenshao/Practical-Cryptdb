a=`cat all`

final=""
arr=($a)
for item in ${arr[@]};do
    final+=" "
    final+=${item}
done

echo ${final}
