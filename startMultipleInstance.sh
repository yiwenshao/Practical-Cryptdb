base=opnow
objDir=main #source
numOfInstance=4
command="sleep 1;echo hehe > log.log &"
rm -rf $base

#create directory
for((i=1;i<=$numOfInstance;i++));do
    mkdir -p $base/ins$i
    echo "copy source" $i
    cp -r $objDir/* $base/ins$i
    echo "copy mtl " $i
    cp -r mtl $base/ins$i
done

for((i=1;i<=$numOfInstance;i++));do
    cd $base/ins$i
    echo "start instance " $i
    eval $command
    cd -
done

