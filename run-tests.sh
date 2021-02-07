TEMP=".temp"

for file in test/*.bl; do
    cat $file | grep "# = .*" | cut -d " " -f 3 > $TEMP.1
    ./basil $file &> $TEMP.2
    if diff -q $TEMP.1 $TEMP.2 &> /dev/null; then
        echo "Test '$file' passed!"
    else
        echo "Test '$file' failed. Diff:"
        sdiff -sd $TEMP.1 $TEMP.2
    fi
    rm $TEMP.1 $TEMP.2
done