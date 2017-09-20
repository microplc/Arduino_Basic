10 rem Show digital pins
20 cls
30 cursor 0,0
40 let n=2
50 read n,v
60 print "D",n," = ",v
70 let n=n+1
80 if n<14,50
90 goto 30
