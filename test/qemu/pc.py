import struct,sys
d=open(sys.argv[1],'rb').read();o=24
def cs(b):
    if len(b)%2:b+=b'\0'
    s=sum(struct.unpack('!%dH'%(len(b)//2),b));
    while s>>16:s=(s&0xffff)+(s>>16)
    return (~s)&0xffff
while o<len(d):
    ts,tu,il,ol=struct.unpack('<IIII',d[o:o+16]);p=d[o+16:o+16+il];o+=16+il
    t=struct.unpack('!H',p[12:14])[0]
    if t==0x806: print('ARP op',p[21],'%d.%d.%d.%d->%d.%d.%d.%d'%tuple(p[28:32]+p[38:42]));continue
    ip=p[14:];hl=(ip[0]&15)*4;tl=struct.unpack('!H',ip[2:4])[0];pr=ip[9]
    s='%d.%d.%d.%d'%tuple(ip[12:16]);dd='%d.%d.%d.%d'%tuple(ip[16:20])
    ipok=cs(ip[:hl])==0
    if pr==6:
        t_=ip[hl:tl];sp,dp,seq,ack=struct.unpack('!HHII',t_[:12]);fl=t_[13];w=struct.unpack('!H',t_[14:16])[0]
        ph=ip[12:20]+struct.pack('!BBH',0,6,len(t_));ok=cs(ph+t_)==0
        dl=len(t_)-(t_[12]>>4)*4
        print('%.3f TCP %s:%d>%s:%d fl=%02x seq=%d ack=%d w=%d len=%d ipck=%s tcpck=%s %r'%(ts+tu/1e6,s,sp,dd,dp,fl,seq,ack,w,dl,ipok,ok,t_[(t_[12]>>4)*4:][:40]))
    else: print('IP',pr,s,dd,ipok)
