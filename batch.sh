BIN=openempires
XRES=1300
YRES=720
USERS=4
CIVS=4
./$BIN --server --users $USERS &
SERVER_PID=$!
for (( i = 0; i < $USERS; i++ ))
do
    D=20
    X=$(($XRES - $D * i))
    Y=$(($YRES - $D * i))
    CIV=$((i % $CIVS))
    ./$BIN --xres $X --yres $Y --civ $CIV &
    sleep 0.1
done
LAST_PID=$!
wait $LAST_PID
kill $SERVER_PID
