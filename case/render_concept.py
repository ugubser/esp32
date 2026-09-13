"""Create a measured preview directly from the current CAD solids."""
from concept import build, OUT
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def rasterize(faces):
    # Orthographic depth buffer prevents large CAD triangles from covering
    # nearer surfaces merely because their centroids sort behind another face.
    w,h=1100,990
    rgb=np.empty((h,w,3));rgb[:]=np.array([243,241,235])/255
    zbuffer=np.full((h,w),np.inf)
    for points,color,z in faces:
        p=np.column_stack(((points[:,0]+80)/167*w,(62-points[:,1])/150*h))
        x0=max(0,int(np.floor(p[:,0].min())));x1=min(w,int(np.ceil(p[:,0].max()))+1)
        y0=max(0,int(np.floor(p[:,1].min())));y1=min(h,int(np.ceil(p[:,1].max()))+1)
        if x1<=x0 or y1<=y0:continue
        den=(p[1,1]-p[2,1])*(p[0,0]-p[2,0])+(p[2,0]-p[1,0])*(p[0,1]-p[2,1])
        if abs(den)<1e-9:continue
        yy,xx=np.ogrid[y0:y1,x0:x1];xx=xx+.5;yy=yy+.5
        a=((p[1,1]-p[2,1])*(xx-p[2,0])+(p[2,0]-p[1,0])*(yy-p[2,1]))/den
        b=((p[2,1]-p[0,1])*(xx-p[2,0])+(p[0,0]-p[2,0])*(yy-p[2,1]))/den
        c=1-a-b;d=a*z[0]+b*z[1]+c*z[2]
        buf=zbuffer[y0:y1,x0:x1]
        mask=(a>=-1e-7)&(b>=-1e-7)&(c>=-1e-7)&(d<buf)
        buf[mask]=d[mask];rgb[y0:y1,x0:x1][mask]=color
    return rgb


def render():
    parts=build()
    fig, axes=plt.subplots(1,2,figsize=(12,7),facecolor='#f3f1eb')
    palette={'front':(.23,.25,.29),'rear':(.52,.55,.60),
             'screen':(.07,.12,.19),'speaker':(.65,.47,.78)}
    depth=np.array([-.48,-.32,1])
    light=np.array([-.3,-.4,-.866])
    for ax,names,title in zip(axes,[['rear','front','screen','speaker'],['rear','speaker']],
                             ['Front / speaker grille','Rear shell / open speaker and cable space']):
        faces=[]
        for name in names:
            vertices,indices=parts[name].val().tessellate(.12,.16)
            v=np.array([[p.x,p.y,p.z] for p in vertices])
            for index in indices:
                t=v[list(index)]
                normal=np.cross(t[1]-t[0],t[2]-t[0])
                normal/=max(np.linalg.norm(normal),1e-12)
                base=(.10,.10,.12) if name=='speaker' and len(names)==4 else palette[name]
                color=np.array(base)*(.68+.32*abs(normal@light))
                projected=np.column_stack((t[:,0]+.48*t[:,2],t[:,1]+.32*t[:,2]))
                faces.append((projected,color,t@depth))
        ax.imshow(rasterize(faces),extent=(-80,87,-88,62),origin='upper')
        ax.set_xlim(-80,87);ax.set_ylim(-88,62);ax.set_aspect('equal');ax.axis('off')
        ax.set_title(title,fontsize=12,color='#242735',pad=12)
    fig.suptitle('FNK0115Q  /  full-width speaker bay',fontsize=22,color='#242735',y=.96)
    fig.text(.5,.115,'143 × 124 × 18 mm overall  •  open cable route  •  flat adhesive back',
             ha='center',fontsize=12,color='#242735')
    fig.text(.5,.068,'18 mm depth = 14 mm padded device allowance + 4 mm. Adhesive strips excluded.',
             ha='center',fontsize=11,color='#625971')
    fig.text(.5,.028,'Inspection draft — board retention and case closure are still unfinished.',
             ha='center',fontsize=10,color='#625971')
    fig.subplots_adjust(left=.025,right=.975,top=.84,bottom=.17,wspace=.04)
    OUT.mkdir(exist_ok=True)
    fig.savefig(OUT/'case-preview.png',dpi=160)


if __name__=='__main__':
    render()
