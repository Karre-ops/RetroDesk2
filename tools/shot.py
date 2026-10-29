# shot.py SHOT.BIN cga|vga|hgc out.png  - 1-Bit-Bildspeicher nach PNG
import sys
from PIL import Image
d=open(sys.argv[1],'rb').read(); mode=sys.argv[2]
w,h,bpr={'cga':(640,200,80),'vga':(640,480,80),'hgc':(720,348,90)}[mode]
im=Image.new('1',(w,h))
px=im.load()
for y in range(h):
    off={'cga':(0x2000 if y&1 else 0)+(y>>1)*80,'vga':y*80,'hgc':(y&3)*0x2000+(y>>2)*90}[mode]
    for xb in range(bpr):
        b=d[off+xb]
        for k in range(8):
            px[xb*8+k,y]=255 if b&(0x80>>k) else 0
im=im.convert('RGB')
im=im.resize({'cga':(960,600),'vga':(960,720),'hgc':(1080,783)}[mode],Image.NEAREST)
im.save(sys.argv[3])
