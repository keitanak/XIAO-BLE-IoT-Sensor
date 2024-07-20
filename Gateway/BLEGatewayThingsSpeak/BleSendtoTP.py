# -*- coding: utf-8 -*-
# 環境センサーは10秒間隔でのアドバタイズ想定。
# CO2センサ付きと、CO2センサ無しの1台づつ構成を想定
# 10秒間スキャンして、取得データを60秒毎に、6回分の平均値計算してThingSpeakに送信

from bluepy.btle import Peripheral, DefaultDelegate, Scanner, BTLEException, UUID
import bluepy.btle
import sys
import struct
from datetime import datetime,timedelta
import argparse
import requests
from requests.exceptions import RequestException, ConnectionError, HTTPError, Timeout
import time
import numpy as np

thingsdata = [[0,0,0,0,0,0,0,0,0] for x in range(6)]
# Temp(out),Hum(out),Press(out),Bat(out),Temp(In),Hum(In),Press(In),CO2(In)
i = 0

blelife={}

# Put your thingsspeak API key.
thingspeak_api_key = 'XXXXXXXXXXXXX'

Debugging = False
def DBG(*args):
    if Debugging:
        msg = " ".join([str(a) for a in args])
        print(msg)
        sys.stdout.flush()

Verbose = True
def MSG(*args):
    if Verbose:
        msg = " ".join([str(a) for a in args])
        print(msg)
        sys.stdout.flush()

def post_thingspeak(value1, value2, value3, value4, value5, value6, value7, value8):
    # Temp(out),Hum(out),Press(out),Bat(out),Temp(In),Hum(In),Press(In),CO2(In)
    url = 'https://api.thingspeak.com/update'
    params = {
        'api_key': thingspeak_api_key,
        'field1': value1, #
        'field2': value2, #
        'field3': value3, #
        'field4': value4, #
        'field5': value5, #
        'field6': value6, #
        'field7': value7, #
        'field8': value8 #
    }
    try:
        r = requests.get(url, params=params)
    except ConnectionError as ce:
        print("Connection Error:", ce)
    except HTTPError as he:
        print("HTTP Error:", he)
    except Timeout as te:
        print("Timeout Error:", te)
    except RequestException as re:
        print("Error:", re)
    return r.status_code

class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)
        self.lastseq = None
        self.lasttime = datetime.fromtimestamp(0)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev or isNewData:
            for (adtype, desc, value) in dev.getScanData():
                if desc == '16b Service Data' and value[0:4] == 'befc' :
                    bleid=int(value[5:9],16)
                    if blelife.get(bleid) == None :
                        delta = timedelta(seconds=10)
                    else:
                        delta = datetime.now() - blelife[bleid]
                    if delta.total_seconds() > 9:
                        blelife[bleid] = datetime.now()
                        now = datetime.now()
                        if len(value) == 42:
                            temp = int(value[16:20], 16) *0.01
                            humid = int(value[22:26], 16) *0.01
                            press = int(value[28:32], 16) *0.1
                            bat = int(value[34:36], 16) * 0.1
                            co2 = int(value[38:42], 16)
                            # Temp(out),Hum(out),Press(out),Bat(out),Temp(In),Hum(In),Press(In),CO2(In)
                            thingsdata[i][5] = temp
                            thingsdata[i][6] = humid
                            thingsdata[i][7] = press
                            thingsdata[i][8] = co2
                        else:
                            temp = int(value[16:20], 16) *0.01
                            humid = int(value[22:26], 16) *0.01
                            press = int(value[28:32], 16) *0.1
                            bat = int(value[34:36], 16) * 0.1
                            # Temp(out),Hum(out),Press(out),Bat(out),Temp(In),Hum(In),Press(In),CO2(In),Bat(in)
                            thingsdata[i][1] = temp
                            thingsdata[i][2] = humid
                            thingsdata[i][3] = press
                            thingsdata[i][4] = bat

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d',action='store_true', help='debug msg on')

    args = parser.parse_args(sys.argv[1:])

    global Debugging,i
    Debugging = args.d
    bluepy.btle.Debugging = args.d

    scanner = Scanner().withDelegate(ScanDelegate())
    while True:
        for i in range(6):
            try:
                scanner.scan(10.0) # スキャンする。デバイスを見つけた後の処理はScanDelegateに任せる
            except BTLEException:
                MSG('BTLE Exception while scannning.')
        print(datetime.now())
        d = np.mean(thingsdata, axis=0)
        print(d)
        p = post_thingspeak(d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8])
        print("ThingSpeak : %d" % (p))

if __name__ == "__main__":
    main()