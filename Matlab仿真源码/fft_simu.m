a=0:20*pi/512:20*pi-20*pi/512;
x=32768*sin(a);
res=fft(x);
L=abs(res);
res(11)
res(503)
num2str(int16(x),'%d,')

