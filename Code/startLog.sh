#!/usr/bin/env bash

# Set weatherStation to TRUE at init
export weatherStation=TRUE

# Set Log Location
logLocation="$HOME/IoTWeather"

# Set device Hostname/IP
weatherStationHN="http://WeatherStation.local/data"

# Check if weatherStation is set to TRUE before entering the loop
while [[ "$weatherStation" == "TRUE" ]]
do
    # Log the data with the specified format
    mkdir -p $logLocation/$(date +"%Y/%m") && echo $(date +"(%T) %c (%s)%t-->%t"; curl -s $weatherStationHN) >> $logLocation/$(date +"%Y/%m/%d").log

    # Sleep for 10 seconds
    sleep 10

    # Check the value of weatherStation again to decide whether to continue or break the loop
    if [[ "$weatherStation" != "TRUE" ]]; then
        break
    fi
done

