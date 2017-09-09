#!/bin/bash
wget "http://192.168.1.123/?message=OK&led1=000000&led2=000000&led3=000000&led4=110000" -O /dev/null > /dev/null
sleep 1
wget "http://192.168.1.123/?message=OH&led1=000000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 1
wget "http://192.168.1.123/?message=Hi&led1=000000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 1
wget "http://192.168.1.123/?message=I%20have%20been%20UPGRADED...%20%20%20&led1=000000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 8
wget "http://192.168.1.123/?message=RGB%20BABY!%20%20%20&led1=110000&led2=001100&led3=000011&led4=030303" -O /dev/null > /dev/null
sleep 4
wget "http://192.168.1.123/?message=%20%20&led1=000000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 0.5
wget "http://192.168.1.123/?message=%20%20&led1=000000&led2=000000&led3=000000&led4=110000" -O /dev/null > /dev/null
sleep 0.5
wget "http://192.168.1.123/?message=%20%20&led1=000000&led2=000000&led3=110000&led4=000000" -O /dev/null > /dev/null
sleep 0.5
wget "http://192.168.1.123/?message=%20%20&led1=000000&led2=110000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 0.5
wget "http://192.168.1.123/?message=%20%20&led1=110000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 0.5
wget "http://192.168.1.123/?message=Knock%20Knock%20NEO...&led1=000000&led2=000000&led3=000000&led4=000000" -O /dev/null > /dev/null
sleep 6
wget "http://192.168.1.123/?matrix=1" -O /dev/null > /dev/null
