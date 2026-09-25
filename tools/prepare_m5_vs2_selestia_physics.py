"""Offline, source-bound Kawaii initial configuration; does not import/run Unreal."""
from pathlib import Path
import hashlib, json, re
import numpy as np

BASE = Path(__file__).resolve().parents[1]
RESEARCH = BASE / 'docs/HarborCity_M5_VS2/research'
AUDIT = RESEARCH / 'SELESTIA_SOURCE_AUDIT.json'
PROBE = BASE / 'docs/HarborCity_M5_VS2/editor_runtime/author_20260923_210909_567_d2dffd06_PhysicsProbe/physics_probe.json'
TARGET = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics'

def quat(q):
    x,y,z,w = np.asarray(q, dtype=float) / np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
        [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
        [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])

def vector(d): return np.array([d[k] for k in ('x','y','z')], dtype=float)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def clean(name): return name.replace('.', '_')

def main():
    audit = json.loads(AUDIT.read_text(encoding='utf-8'))
    probe = json.loads(PROBE.read_text(encoding='utf-8'))
    assert probe['status'] == 'PROBE_COMPLETE_NOT_PHYSICS_ACCEPTANCE'
    assert probe['package_dirty_flags_unchanged'] is True
    prefab = next(x for x in audit['prefabs'] if x['path'] == 'Assets/SELESTIA/Prefab/SELESTIA_lilToon.prefab')
    entries = prefab['selected_bind_transforms']['entries']
    assert len(entries) == 208
    bones = {x['name']:x for x in probe['imported_reference_bones']}
    by_name = {x['name']:x for x in entries}
    assert len(by_name) == len(entries)
    pairs = [(x,bones[clean(x['name'])]) for x in entries if clean(x['name']) in bones]
    X = np.array([np.array(x['world_matrix_column_vector'])[:3,3] for x,b in pairs])
    Y = np.array([b['component_refpose']['translation_cm'] for x,b in pairs])
    xc,yc = X.mean(0),Y.mean(0)
    u,s,vt = np.linalg.svd((X-xc).T@(Y-yc)); rotation = vt.T@u.T
    scale = s.sum()/np.square(X-xc).sum(); translation = yc-scale*rotation@xc
    fit_error = np.linalg.norm((scale*rotation@X.T).T+translation-Y,axis=1)
    # Snap only AFTER measuring the native probe's correspondence.
    axes = np.array([[-1.,0,0],[0,0,1],[0,1,0]])
    canonical_error = np.linalg.norm((100*axes@X.T).T-Y,axis=1)
    assert len(pairs)==195 and np.linalg.det(rotation)>.9999
    assert abs(scale-100)<.001 and canonical_error.max()<.01
    colliders = {}
    for c in prefab['explicit_physbone_colliders']:
        source = by_name[c['root']]; m = np.array(source['world_matrix_column_vector'])
        anchor = source['nearest_imported_ancestor_including_self']['ue_probe_name']
        ref = bones[anchor]['component_refpose']; ar = quat(ref['quaternion_xyzw']); ap=np.array(ref['translation_cm'])
        assert np.max(np.abs(np.array(ref['scale'])-1))<1e-5
        linear=m[:3,:3]; scales=np.linalg.norm(linear,axis=0)
        assert max(scales)-min(scales)<.001 and abs(scales.mean()-1)<.001
        st=c['settings']; assert st['insideBounds']==0 and st['shapeType'] in (0,1)
        center = 100*axes@(linear@vector(st['position'])+m[:3,3])
        q=st['rotation']; source_axis=linear@quat([q[k] for k in ('x','y','z','w')])@np.array([0.,1,0])
        axis=axes@source_axis;axis/=np.linalg.norm(axis)
        local_center=ar.T@(center-ap);local_axis=ar.T@axis
        # Native capsules cannot be elliptical. The largest singular scale encloses
        # the tiny source nonuniform thigh scale; retain actual axis segment scale.
        radius=st['radius']*100*float(np.linalg.svd(linear,compute_uv=False).max())
        length=max(0.,(st['height']-2*st['radius'])*100*float(np.linalg.norm(source_axis))) if st['shapeType']==1 else 0.
        error=np.linalg.norm(ap+ar@local_center-center)
        assert error<1e-8 and np.linalg.norm(ar@local_axis-axis)<1e-8
        colliders[c['component_id']]={'source_id':c['component_id'],'source_root':c['root'],
            'driving_bone':anchor,'offset_cm':local_center.tolist(),'axis_local':local_axis.tolist(),
            'radius_cm':radius,'length_cm':length,'shape':'capsule' if length>0 else 'sphere',
            'source_world_scale':scales.tolist(),'center_ue_cs_cm':center.tolist(),
            'axis_ue_cs':axis.tolist(),'roundtrip_error_cm':float(error)}
    groups={};excluded=[];seen=set();expected={};directions=[]
    families=[('Hair_back_long','LongHair',.24,.14,35),('Hair_side2','SideLong',.28,.18,30),
        ('Hair_back_short','BackShort',.3,.20,25),('Hair_tail','HairTail',.28,.18,30),
        ('Hair_front','Front',.38,.25,20),('Hair_side1','SideShort',.38,.25,20),
        ('Hair_ahoge','Ahoge',.35,.22,10),('Skirt_','Skirt',.35,.25,25),
        ('Hair_ribbon','HairRibbon',.3,.18,30),('Chest_ribbon','ChestRibbon',.4,.28,25),
        ('LowerLeg_ribbon','LegRibbon',.35,.22,25)]
    for c in prefab['physbones']:
        family=next((f for f in families if c['root'].startswith(f[0])),None)
        if not family or not c['enabled']:
            excluded.append({'root':c['root'],'reason':'separate static Halo has no imported body bones' if c['root'].startswith('SELESTIA_halo') else 'disabled or anatomy outside authorized secondary scope'})
            continue
        names=[clean(n) for n in c['chain_bones']]
        assert all(n in bones for n in names) and not seen.intersection(names)
        seen.update(names)
        # Confirm complete native subtree, so Kawaii recursion cannot touch unapproved bones.
        root_index=bones[names[0]]['index']; descendants=[]
        for b in bones.values():
            i=b['index']
            while i>=0 and i!=root_index: i=probe['imported_reference_bones'][i]['parent_index']
            if i==root_index:descendants.append(b['name'])
        assert set(descendants)==set(names),(names[0],descendants,names)
        for n in names:
            b=bones[n];expected[n]={'name':n,'parent_index':b['parent_index'],**b['component_refpose']}
        child=bones[names[1]]; direction=np.array(child['direction_in_parent_axes'])
        assert direction[1]<-.9999
        directions.append({'root':names[0],'first_child_direction_local':direction.tolist()})
        key=family[1];g=groups.setdefault(key,{'name':key,'roots':[],'chains':[], 'collider_ids':set(),
            'damping':family[2],'stiffness':family[3],'angle':family[4],'radius_cm':c['settings']['radius']*100})
        assert abs(g['radius_cm']-c['settings']['radius']*100)<1e-5
        g['roots'].append(names[0]);g['chains'].append({'source_root':c['root'],'bones':names})
        g['collider_ids'].update(c['collider_component_ids'])
    assert len(seen)>100 and sum(len(g['roots']) for g in groups.values())==42
    output_groups=[];penetrations=[]
    for name in [f[1] for f in families]:
        g=groups[name];g['roots'].sort();g['collider_ids']=sorted(g['collider_ids'])
        limits=[colliders[i] for i in g['collider_ids']]
        # A source reference-pose overlap is a tuning diagnostic, NOT a mesh penetration test.
        for chain in g['chains']:
            for n in chain['bones'][1:]:
                p=np.array(bones[n]['component_refpose']['translation_cm'])
                for c in limits:
                    center=np.array(c['center_ue_cs_cm']);axis=np.array(c['axis_ue_cs'])
                    t=np.clip((p-center)@axis,-c['length_cm']/2,c['length_cm']/2)
                    clearance=float(np.linalg.norm(p-center-t*axis)-c['radius_cm']-g['radius_cm'])
                    if clearance<0:penetrations.append({'group':name,'bone':n,'collider':c['source_root'],'clearance_cm':clearance})
        props={'RootBone':{'BoneName':g['roots'][0]},
            'AdditionalRootBones':[{'RootBone':{'BoneName':n}} for n in g['roots'][1:]],
            'BoneForwardAxis':'Y_Negative','DummyBoneLength':0,'BoneSubdivisionCount':1,
            'bBoneSubdivisionCollisionOnly':True,'SimulationSpace':'ComponentSpace','TargetFramerate':60,
            'PhysicsSettings':{'Damping':g['damping'],'Stiffness':g['stiffness'],
                'WorldDampingLocation':.85,'WorldDampingRotation':.85,'Radius':g['radius_cm'],'LimitAngle':g['angle']},
            'Gravity':{'X':0,'Y':0,'Z':0},'bUseDefaultGravityZProjectSetting':False,
            'bUseWorldSpaceGravity':True,'bUseLegacyGravity':False,'bEnableWind':False,
            'bAllowWorldCollision':False,'bUseSharedCollision':False,
            'TeleportDistanceThreshold':150,'TeleportRotationThreshold':90,'Alpha':1,'LODThreshold':-1}
        output_groups.append({'name':name,'roots':g['roots'],'chains':g['chains'],
            'source_collider_ids':g['collider_ids'],'colliders':limits,'node_properties':props})
    data={'schema':1,'status':'OFFLINE_COORDINATE_VALIDATED_RUNTIME_NOT_RUN',
        'source_audit_sha256':sha(AUDIT),'native_probe_sha256':sha(PROBE),'prefab_path':prefab['path'],
        'target_blueprint':TARGET,'mesh':probe['mesh'],'skeleton':probe['skeleton'],
        'coordinate_validation':{'selected_transforms':208,'paired_bones':len(pairs),
            'fitted_scale':float(scale),'fitted_rotation':rotation.tolist(),'fitted_translation_cm':translation.tolist(),
            'fit_rms_cm':float(np.sqrt(np.mean(fit_error**2))),'fit_max_cm':float(fit_error.max()),
            'applied_axis_matrix':axes.tolist(),'applied_scale':100,'canonical_max_cm':float(canonical_error.max()),
            'actor_scale_applied':False,'first_child_directions':directions},
        'expected_reference_bones':list(expected.values()),'converted_colliders':list(colliders.values()),
        'groups':output_groups,'excluded_chains':excluded,'root_count':42,
        'reference_pose_collider_overlaps':penetrations,
        'limitations':['Kawaii settings are conservative initial profiles, not equivalent PhysBone solver/curves.',
            'Source full curve tangents unavailable in audit; not invented. Radius uses measured source constant.',
            'LimitAngle is a cone; source polar/hinge and local limitRotation are not reproduced.',
            'Source has no sleeve-specific chain; sleeves remain base skinned arm animation.',
            'Halo is separate static geometry; no body Halo bones invented.',
            'Bone-point/reference collision diagnostics cannot prove moving surface non-penetration.',
            'Native author/compile/save, movement, collision and visual acceptance NOT_RUN.']}
    path=RESEARCH/'SELESTIA_KAWAII_INITIAL_CONFIG.json'
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'output':str(path),'groups':len(groups),'roots':42,'fit_max_cm':float(fit_error.max()),
        'canonical_max_cm':float(canonical_error.max()),'reference_overlaps':len(penetrations),'sha256':sha(path)}))

if __name__=='__main__':main()
