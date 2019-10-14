#!/usr/bin/python3

from matplotlib import pyplot as plt
from numpy import genfromtxt
import numpy


data = genfromtxt("raw_payload_selector_total.txt", delimiter=',')

# variables from data
rawbytes = data[:,0]
payloadbytes = data[:,1]
selectorbytes = data[:,2]
totalbytes = data[:,3]

plt.plot(rawbytes, totalbytes, 'o')
plt.plot(rawbytes, payloadbytes, 'o')
plt.plot(rawbytes, selectorbytes, 'o')
#plt.plot(rawbytes, rawbytes, 'k-')
plt.ylim([0, 38000])
plt.xlim([0, 650000])

plt.xlabel('raw size (bytes)')
plt.ylabel('compressed size (bytes)')
plt.legend(['total', 'payload','selectors'])
plt.show()

