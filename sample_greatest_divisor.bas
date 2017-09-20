10 rem Greatest common divisor
20 print "Greatest common divisor of a anb b"
30 print "Enter a,b"
40 input a,b
50 if a=b,150
60 if a>b,90
70 let b=b-a
80 goto 100
90 let a=a-b
100 goto 50
150 print "GCD is ",a
160 end
