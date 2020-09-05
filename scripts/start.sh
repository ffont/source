# If python server is running, kill it
PID=`ps -eaf | grep "app.py" | grep -v grep | awk '{print $2}'`
if [[ "" !=  "$PID" ]]; then
  echo "* Killing existing python server with PID $PID"
  kill -9 $PID
fi

# If sushi running, kill it
PID=`ps -eaf | grep "sushi" | grep -v grep | awk '{print $2}'`
if [[ "" !=  "$PID" ]]; then
  echo "* Killing existing python server with PID $PID"
  kill -9 $PID
fi

# Start python server and wait a few seconds
echo "* Starting python server..."
python app.py &

# Start sushi with source plugin
echo "* Starting sushi..."
sushi -r -c /home/mind/source_sushi_config.json &

# Running acconect to connect MIDI interface
echo "* Connecting MIDI interface..."
sleep 10  && aconnect 16 128
