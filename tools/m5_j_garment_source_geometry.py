"""Read-only original VRM geometry audit. Does not modify or redistribute source geometry."""
import json,struct
from pathlib import Path
import numpy as np
P=Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/Models/AvatarSample_J/AvatarSample_J_Studio2140.vrm')
O=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/research/J_GARMENT_SOURCE_GEOMETRY.json')
b=P.read_bytes();assert b[:4]==b'glTF';pos=12;chunks={}
while pos<len(b):
    size,kind=struct.unpack_from('<II',b,pos);chunks[kind]=b[pos+8:pos+8+size];pos+=8+size
g=json.loads(chunks[0x4E4F534A]);binary=chunks[0x004E4942]
def array(i):
    a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];dt=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]);n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
    return np.ndarray((a['count'],n),dtype=dt,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',dt.itemsize*n),dt.itemsize)).copy()
rows=[]
for node in g['nodes']:
    if 'mesh' not in node or 'skin' not in node:continue
    bones=[g['nodes'][i].get('name','') for i in g['skins'][node['skin']]['joints']]
    for p in g['meshes'][node['mesh']]['primitives']:
        name=g['materials'][p['material']]['name']
        if 'N00_002_03_Tops_01_CLOTH_0' not in name:continue
        at=p['attributes'];ix=np.unique(array(p['indices']).reshape(-1)).astype(int)
        v=array(at['POSITION'])[ix]*100;j=array(at['JOINTS_0'])[ix].astype(int);w=array(at['WEIGHTS_0'])[ix];mask=np.vectorize(lambda i:'_Skirt' in bones[i] and 'Coat' not in bones[i])(j)
        frac=(mask*w).sum(1);fixed=frac<1e-5
        rows.append(dict(material=name,vertices=len(v),reference_xyz_min_cm=v.min(0).tolist(),reference_xyz_max_cm=v.max(0).tolist(),
            fixed_zero_skirt_vertices=int(fixed.sum()),fixed_y_height_percentiles_cm=np.percentile(v[fixed,1],[0,10,25,50,75,90,100]).tolist(),
            moving_y_height_percentiles_cm=np.percentile(v[~fixed,1],[0,25,50,75,100]).tolist(),
            zero_skirt_skin_bones=sorted({bones[int(j[i,k])] for i in np.where(fixed)[0] for k in range(4) if w[i,k]>.001})))
O.write_text(json.dumps(dict(source=str(P),scope='Reference VRM coordinates in cm; not final UE pose, no runtime contact claim',garments=rows),indent=2),encoding='utf-8')
print(json.dumps(rows))
