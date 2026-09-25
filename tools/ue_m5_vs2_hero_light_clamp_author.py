"""One source-bound Hero light-limit candidate; root invokes in native UE only.

HeroLightClampApply creates five child MIs and binds only the existing VS2 BP.
HeroLightClampVerify is a separate-process readback, with no saves.
HeroLightClampRestore restores the original five references by native save;
original bytes remain in the private backup. No map/light/source-graph writes.
Native author PASS is not rendered/visual acceptance.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import os
import re
import shutil
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'
DOC=WORK/'docs/HarborCity_M5_VS2'
TARGET='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
CLASS=TARGET+'.BP_M5VS2_Selestia_C'
HERO_OWNER='HarborCity_M5_VS2_HeroBinding'
OWNER='HarborCity_M5_VS2_HeroLightClamp'
KEY='HarborCityOwnedBy'
SLOTS=('hair','body','face','option','costume')
BASELINE_SHA='42ecd5b3a74c1d12aceeae5eac8559b4ac552cb931cba1b0e6f30f3bc7128940'
SOURCE_ROOT='/Game/HarborCity/M5VS2/HeroLightChroma/Review_c65c8f5c593f'
SOURCE_AUTHOR=DOC/'editor_runtime/author_20260924_152537_400_9e7e0df0_HeroLightChroma/author_result.json'
SOURCE_AUTHOR_SHA='e2ac7bf4e74cf8d766efc6f6766e45ae2bcb12aad6d8e435812d307012c1915e'
BASE_RUNTIME=DOC/'editor_runtime/20260925_010507_159_dc629c50_corner_game/CornerReview_20260925_010527_83B3AA5B/corner_review.json'
BACKUPS=Path('E:/GameDev/Assets/HarborCity/M5_VS2/HeroSelestia/NativeBackups').resolve()
A,E=U.EditorAssetLibrary,U.MaterialEditingLibrary

def argument(name):
    v=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(v)==1,'Exactly one '+name+' required'
    return v[0][0] or v[0][1]

OUT=Path(argument('M5EvidenceDir')).resolve()
PHASE=argument('M5AuthorPhase')
assert PHASE in ('HeroLightClampApply','HeroLightClampVerify','HeroLightClampRestore')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
TOKEN=hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
DEST='/Game/HarborCity/M5VS2/HeroLightClamp/Review_'+TOKEN
PROTECTED,TREES,ALLOWED={},{},set()
R=dict(schema='HarborCity.M5VS2.HeroLightClampAuthor.v1',status='RUNNING',phase=PHASE,process_id=os.getpid(),
       checks=[],inputs={},assets=[],transitions=[],applied=False,target=TARGET,target_directory=DEST,
       source_material_root=SOURCE_ROOT,candidate_max=.75,preserved_min=.05,preserved_chroma=.5,
       runtime='NOT_RUN',visual='USER_REVIEW',fresh_process_disk_readback='NOT_RUN',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       scope='Five new child MIs override only HeroLightMax, and one BP body material array. No graph/texture/pose/light/map/NPC/animation changes.',
       selection_basis='Actual dc629c50 afternoon portrait loses face/hair contrast; .75 is a bounded art candidate, not a measured solution or global exposure adjustment.',
       limitations=['Clamp acts only above its bound; .75 does not guarantee 25 percent lower final pixels.',
                    'Both BaseColor and self-lit function limits are inspected; deferred lighting, tonemapping and unchanged face emission remain.',
                    'Native NullRHI readback is not per-material renderer readiness or visual acceptance.',
                    'Only actual same-corner portraits after binding can assess improvement.'])


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')

def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    if not ok:
        dump()
        raise RuntimeError(name + ': ' + repr(observed))

def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def binding(file):
    file = Path(file).resolve()
    return dict(path=str(file), sha256=sha(file))

def package(obj):
    return (obj.get_path_name() if isinstance(obj, U.Object) else str(obj)).split('.')[0]

def asset_file(path, extension='.uasset'):
    name = package(path)
    assert name.startswith('/Game/HarborCity/') and '..' not in name
    return (PROJECT / 'Content' / (name.removeprefix('/Game/') + extension)).resolve()

def protect(file, expected=None):
    file = Path(file).resolve()
    check('protected input exists', file.is_file(), str(file))
    actual = sha(file)
    check('exact source hash', expected is None or actual == expected, str(file))
    PROTECTED[str(file)] = actual

def tree(folder):
    folder = Path(folder).resolve()
    # pathlib's WindowsPath ordering is case-folded, str ordering is not.
    # Use the same explicit string ordering both here and in the final guard.
    files = sorted((p.resolve() for p in folder.rglob('*') if p.is_file()), key=str)
    check('protected tree exists', folder.is_dir() and bool(files), str(folder))
    TREES[str(folder)] = [str(p) for p in files]
    for file in files:
        protect(file)

def load(path):
    obj = A.load_asset(path)
    check('native asset load', obj is not None, path)
    return obj

def path_of(obj):
    return obj.get_path_name() if obj is not None else None

def value(obj):
    if obj is None or isinstance(obj, (str, bool, int, float)):
        return obj
    if isinstance(obj, U.Object):
        return obj.get_path_name()
    if isinstance(obj, (list, tuple, U.Array)):
        return [value(x) for x in obj]
    if isinstance(obj, U.Vector):
        return [float(obj.x), float(obj.y), float(obj.z)]
    if isinstance(obj, U.Rotator):
        return [float(obj.pitch), float(obj.yaw), float(obj.roll)]
    # UE's StructBase.__str__ includes a memory address; its native export_text
    # explicitly uses PPF_None and is suitable for cross-process equality.
    if isinstance(obj, U.StructBase):
        return obj.export_text()
    # Reflected enum/name values have stable text, without UObject addresses.
    return str(obj)

def properties(obj, names):
    return {name: value(obj.get_editor_property(name)) for name in names}

def dirty():
    return sorted({p.get_path_name() for fn in (
        U.EditorLoadingAndSavingUtils.get_dirty_content_packages,
        U.EditorLoadingAndSavingUtils.get_dirty_map_packages) for p in fn()})

def hero_objects():
    bp = load(TARGET)
    check('exact owned Hero Blueprint', isinstance(bp, U.Blueprint) and A.get_metadata_tag(bp, KEY) == HERO_OWNER)
    cls = U.load_class(None, CLASS)
    check('native Hero generated class', cls is not None)
    cdo = U.get_default_object(cls)
    body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    check('native body exists', body is not None)
    check('no candidate point-light components', not cdo.get_components_by_class(U.PointLightComponent))
    return bp, cdo, body

def snapshot(cdo, body):
    movement = cdo.get_component_by_class(U.CharacterMovementComponent)
    capsule = cdo.get_component_by_class(U.CapsuleComponent)
    combat = cdo.get_component_by_class(U.HCM4CombatComponent)
    presentation = cdo.get_component_by_class(U.HCM4R2PresentationComponent)
    check('existing gameplay components present', all(x is not None for x in (movement, capsule, combat, presentation)))
    mesh = body.get_editor_property('skeletal_mesh_asset')
    slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('materials')]
    check('actual ordered five Selestia slots', [s.removeprefix('Selestia_') for s in slots] == list(SLOTS), slots)
    check('five effective body materials', body.get_num_materials() == 5)
    keep = dict(
        character_class=cdo.get_class().get_path_name(),
        character=properties(cdo, ('walk_speed', 'sprint_speed', 'base_eye_height', 'crouched_eye_height', 'use_controller_rotation_yaw')),
        component_inventory=sorted([x.get_name(), x.get_class().get_path_name()] for x in cdo.get_components_by_class(U.ActorComponent)),
        body=properties(body, ('skeletal_mesh_asset', 'anim_class', 'animation_mode', 'physics_asset_override',
                              'relative_location', 'relative_rotation', 'relative_scale3d', 'lighting_channels')),
        movement=properties(movement, ('max_walk_speed', 'max_acceleration', 'braking_deceleration_walking',
                                      'gravity_scale', 'rotation_rate', 'orient_rotation_to_movement')),
        capsule=properties(capsule, ('capsule_radius', 'capsule_half_height')),
        combat=properties(combat, ('aim_look_scale', 'ads_magnification', 'aim_transition_seconds', 'magazine_capacity',
                                  'initial_reserve', 'hip_spread_degrees', 'aim_spread_degrees', 'shot_range', 'body_damage',
                                  'head_damage', 'melee_sphere_radius', 'attack_movement_scale', 'combo_reset_seconds',
                                  'attack_durations', 'hit_window_starts', 'hit_window_ends', 'combo_window_starts', 'melee_damage',
                                  'punch_animations', 'player_animation_blueprint', 'pistol_idle_animation', 'pistol_aim_animation',
                                  'pistol_fire_animation', 'pistol_reload_animation', 'player_hit_front_montage')),
        first_person=properties(presentation, ('first_person_arms_override', 'eye_anchor_in_mesh', 'first_person_skinning_radius_cm',
                                              'head_accessory_mesh', 'head_accessory_bone', 'head_accessory_local_transform',
                                              'relaxed_hand_camera', 'relaxed_anatomical_wrists', 'relaxed_hand_swing_scale',
                                              'relaxed_run_hand_camera', 'relaxed_finger_pose', 'unarmed_attack_hand_offset',
                                              'unarmed_attack_lateral_scale')))
    return dict(slot_names=slots, materials=[path_of(body.get_material(i)) for i in range(5)],
                override_materials=[path_of(x) for x in body.get_editor_property('override_materials')], preserved=keep)


def instance_parameters(mi):
    result={}
    for kind in ('scalar','vector','texture','static_switch'):
        names=list(getattr(E,'get_'+kind+'_parameter_names')(mi))
        check('bounded reflected parameter inventory',len(names)<512)
        getter=getattr(E,'get_material_instance_'+kind+'_parameter_value')
        result[kind]={str(n):value(getter(mi,n)) for n in names}
    return result


def base_override_fields(mi):
    """Read full native export; inactive value caches are not override flags.

    UE 5.8 MaterialInstance.cpp UpdateOverridableBaseProperties copies Parent
    getters into inactive values (e.g. BlendMode, TwoSided, clip, UsageFlags).
    Comparing against the unparented factory's values therefore rejects normal
    inheritance. Keep all flags off and compare the resulting effective cache
    with the actual parent, while recording both complete exports.
    """
    raw=mi.get_editor_property('base_property_overrides').export_text()
    check('native base override export is structured',raw.startswith('(') and raw.endswith(')'),raw)
    parts=[];depth=0;start=1
    for index,char in enumerate(raw[1:-1],1):
        if char=='(':depth+=1
        elif char==')':depth-=1
        elif char==',' and depth==0:parts.append(raw[start:index]);start=index+1
    parts.append(raw[start:-1])
    fields=dict(item.split('=',1) for item in parts)
    flags={k:v for k,v in fields.items() if k.startswith('bOverride_')}
    expected={'bOverride_'+x for x in ('OpacityMaskClipValue','BlendMode','ShadingModel','DitheredLODTransition',
        'CastDynamicShadowAsMasked','TwoSided','bIsThinSurface','OutputTranslucentVelocity','bHasPixelAnimation',
        'bEnableTessellation','DisplacementScaling','bEnableDisplacementFade','DisplacementFadeRange',
        'MaxWorldPositionOffsetDisplacement','CompatibleWithLumenCardSharing','UsageFlags')}
    check('complete native base override flag inventory',depth==0 and len(parts)==len(fields) and set(flags)==expected,
          dict(actual=sorted(flags),expected=sorted(expected),export=raw))
    return dict(export=raw,flags=flags,effective_cache={k:v for k,v in fields.items() if k not in flags})


def verify_child_inheritance(mi,parent,stage):
    current_parent=path_of(mi.get_editor_property('parent'));expected_parent=path_of(parent)
    check('exact native child parent '+stage,current_parent==expected_parent,
          dict(actual=current_parent,expected=expected_parent))
    child=base_override_fields(mi);source=base_override_fields(parent)
    inactive={k:('0' if k=='bOverride_UsageFlags' else 'False') for k in child['flags']}
    check('child activates no base-property override '+stage,child['flags']==inactive,
          dict(actual=child['flags'],expected=inactive,actual_export=child['export']))
    check('child inherits actual parent effective base properties '+stage,child['effective_cache']==source['effective_cache'],
          dict(actual=child['effective_cache'],expected=source['effective_cache'],
               child_export=child['export'],parent_export=source['export']))
    return child


def nodes(obj):
    items=list(E.get_material_function_expressions(obj))
    check('bounded unique native graph',0<len(items)<1200 and len({n.get_name() for n in items})==len(items))
    return {n.get_name():n for n in items}


def links(obj,node):
    names=list(E.get_material_expression_input_names(node))
    values=list(E.get_inputs_for_material_function_expression(obj,node))
    check('complete graph input enumeration',len(names)==len(values))
    return {str(n):(v.get_name() if v else None) for n,v in zip(names,values)}


def graph_preflight():
    rows=[]
    for name in ('MF_BaseColor','MF_VrmMToonBase'):
        obj=load(SOURCE_ROOT+'/Materials/Core/'+name)
        check('actual material function class',isinstance(obj,U.MaterialFunction))
        ns=nodes(obj);clamp=ns['MaterialExpressionClamp_1']; edges=links(obj,clamp)
        check('both actual branches clamp summed world light',isinstance(clamp,U.MaterialExpressionClamp)
              and clamp.get_editor_property('clamp_mode')==U.ClampMode.CMODE_CLAMP
              and set(edges)=={'None','Min','Max'} and edges['None']=='MaterialExpressionAdd_7',name)
        parameters={}
        for pin,param,expected in (('Min','HeroLightMin',.05),('Max','HeroLightMax',1.)):
            node=ns[edges[pin]]
            check('actual connected scalar '+name+':'+pin,isinstance(node,U.MaterialExpressionScalarParameter)
                  and str(node.get_editor_property('parameter_name'))==param
                  and abs(float(node.get_editor_property('default_value'))-expected)<1e-6)
            parameters[pin]=dict(node=node.get_name(),parameter=param,default=float(node.get_editor_property('default_value')))
        add=links(obj,ns['MaterialExpressionAdd_7'])
        check('summed directional input present','MaterialExpressionCustom_0' in add.values()
              and 'ResolvedView.DirectionalLightColor' in ns['MaterialExpressionCustom_0'].get_editor_property('code'))
        downstream=links(obj,ns['MaterialExpressionMultiply_23'])
        check('clamp feeds existing multiplier',list(downstream.values())==['MaterialExpressionClamp_1','MaterialExpressionCollectionParameter_1'])
        row=dict(path=package(obj),sha256=sha(asset_file(obj)),clamp_inputs=edges,bounds=parameters,sum_inputs=add,multiplier_inputs=downstream)
        if name=='MF_VrmMToonBase':
            attenuation=ns['MaterialExpressionScalarParameter_5']; response=ns['MaterialExpressionLinearInterpolate_9']
            check('unchanged attenuation source',str(attenuation.get_editor_property('parameter_name'))=='mtoon_LightColorAttenuation'
                  and links(obj,response)==dict(A=None,B=None,Alpha=attenuation.get_name()))
            lit=ns['MaterialExpressionStaticSwitchParameter_3']
            check('actual lit weighting switch',str(lit.get_editor_property('parameter_name'))=='bUseLight'
                  and links(obj,lit)=={'True':response.get_name(),'False':'MaterialExpressionConstant_3'})
            inv=ns['MaterialExpressionOneMinus_4'];ratio=ns['MaterialExpressionLinearInterpolate_5'];custom=ns['MaterialExpressionStaticSwitchParameter_18']
            check('actual base color weighting path',links(obj,inv)=={'None':lit.get_name()}
                  and links(obj,ratio)==dict(A=None,B=None,Alpha=inv.get_name())
                  and str(custom.get_editor_property('parameter_name'))=='bUseCustomBaseColorRate'
                  and links(obj,custom)=={'True':'MaterialExpressionScalarParameter_19','False':ratio.get_name()})
            a,b=(float(response.get_editor_property(k)) for k in ('const_a','const_b'))
            c,d=(float(ratio.get_editor_property(k)) for k in ('const_a','const_b'))
            alpha=a+(b-a)*.1; rate=c+(d-c)*(1-alpha)
            check('known existing approximate lighting split',abs(rate-.135)<1e-6)
            row['branch_weight_calculation']=dict(attenuation=.1,normal_lerp_constants=[a,b],normal_lerp_alpha=alpha,
                base_rate_constants=[c,d],base_color_rate=rate,self_lit_rate=1-rate,
                interpretation='Algebra of existing graph, not separately measured per-pixel contribution.')
        rows.append(row)
    R['native_graph_preflight']=rows


def source_preflight():
    protect(SOURCE_AUTHOR,SOURCE_AUTHOR_SHA)
    author=json.loads(SOURCE_AUTHOR.read_text('utf-8-sig'))
    check('exact successful source author',author['status']=='PASS' and author['target_directory']==SOURCE_ROOT)
    for row in author['assets']:
        protect(asset_file(row['path'],'.umap' if row['path']==author['map'] else '.uasset'),row['sha256'])
    mapping={r['slot']:r['path'] for r in author['materials']['Chroma_050']}
    check('five exact source slots',set(mapping)==set(SLOTS) and len(mapping)==5)
    mats=[load(mapping[s]) for s in SLOTS]
    for s,m in zip(SLOTS,mats):
        check('exact source constant instance',isinstance(m,U.MaterialInstanceConstant)
              and package(m)==SOURCE_ROOT+'/Materials/Chroma_050/MI_MToon_'+s
              and A.get_metadata_tag(m,KEY)=='HarborCity_M5_VS2_HeroLightChroma')
        p=instance_parameters(m)
        for name,wanted in (('HeroLightMax',1.),('HeroLightMin',.05),('HeroLightChroma',.5),('mtoon_LightColorAttenuation',.1)):
            check('current inherited parameter '+s+':'+name,abs(p['scalar'][name]-wanted)<1e-6)
        check('lighting split not bypassed',p['static_switch']['bUseLight'] and not p['static_switch']['bUseCustomBaseColorRate'])
    graph_preflight()
    protect(BASE_RUNTIME)
    runtime=json.loads(BASE_RUNTIME.read_text('utf-8-sig'))
    check('actual source portrait baseline',runtime['status']=='PASS' and runtime['profile']=='Portraits'
          and runtime['user_stop_latched'] is False and runtime['actual_verified_png_count']==6)
    for i in (0,3):
        row=runtime['captures'][i]
        people=row['actual_at_request']['characters_unmodified_by_director']
        hero=[x for x in people if x['blueprint_class']==CLASS]
        check('baseline portrait exact current materials',len(hero)==1 and hero[0]['actual_materials']==[path_of(m) for m in mats]
              and row['status']=='PASS' and row['final_view_matches_locked_camera'] is True)
        protect(Path(row['file']))
    R['inputs'].update(script=binding(Path(__file__)),source_author=binding(SOURCE_AUTHOR),baseline_portraits=binding(BASE_RUNTIME))
    return mats


def apply(sources):
    prior=DOC/'editor_runtime/author_20260925_013226_167_5d527778_HeroLightClampApply/author_result.json'
    protect(prior,'90e3fddbb77e6f97d5c8c4ddbd817103a1de1612eef4487db1bd7610389bf7e5')
    failed=json.loads(prior.read_text('utf-8-sig'))
    failed_dir=asset_file(failed['target_directory']+'/placeholder').parent
    check('prior failure wrote no asset and did not bind BP',failed['status']=='FAIL' and failed['applied'] is False
          and failed['assets']==[] and failed['transitions']==[] and not failed_dir.exists()
          and failed['preservation']['allowed_changed_files']==[]
          and failed['preservation']['unexpected_changed_files']==[] and failed['preservation']['tree_membership_changes']==[],
          dict(report=str(prior),assets=failed['assets'],applied=failed['applied'],candidate_directory_exists=failed_dir.exists()))
    for file,expected in failed['protected_source_sha256_before'].items():protect(file,expected)
    protect(failed['backup']['path'],BASELINE_SHA)
    R['inputs']['prior_unsaved_failure']=binding(prior)
    check('one exact current BP baseline',sha(asset_file(TARGET))==BASELINE_SHA)
    bp,cdo,body=hero_objects(); before=snapshot(cdo,body)
    check('current five source overrides exact',before['materials']==before['override_materials']==[path_of(m) for m in sources])
    R['baseline_native_cdo']=before
    backup_dir=(BACKUPS/OUT.name).resolve()
    check('fresh private byte backup namespace',backup_dir.is_relative_to(BACKUPS) and not backup_dir.exists())
    backup_dir.mkdir(parents=True,exist_ok=False)
    backup=backup_dir/asset_file(TARGET).name
    shutil.copy2(asset_file(TARGET),backup)
    check('original BP byte backup verified',sha(backup)==BASELINE_SHA)
    R['backup']=binding(backup);dump()
    check('new candidate namespace absent',not asset_file(DEST+'/placeholder').parent.exists() and not A.does_directory_exist(DEST))
    children=[]
    for slot,parent in zip(SLOTS,sources):
        path=DEST+'/MI_HeroLightMax075_'+slot
        check('fresh child MI target',not A.does_asset_exist(path) and not asset_file(path).exists())
        folder,name=path.rsplit('/',1)
        mi=U.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,U.MaterialInstanceConstant,U.MaterialInstanceConstantFactoryNew())
        check('native child MI created',isinstance(mi,U.MaterialInstanceConstant))
        constructor_overrides=base_override_fields(mi)
        E.set_material_instance_parent(mi,parent)
        inherited_overrides=verify_child_inheritance(mi,parent,slot+' after parent assignment')
        R.setdefault('instance_inheritance_diagnostics',[]).append(dict(slot=slot,path=path,
            unparented_factory=constructor_overrides,after_parent_assignment=inherited_overrides,
            note='Inactive cache values may change during native inheritance; the child must enable no base override.'))
        expected=instance_parameters(parent)
        check('new child inherits exact effective source parameters',instance_parameters(mi)==expected)
        setter=E.set_material_instance_scalar_parameter_value(mi,U.Name('HeroLightMax'),.75)
        E.update_material_instance(mi)
        expected['scalar']['HeroLightMax']=.75
        check('only Max changes effective parameters',instance_parameters(mi)==expected)
        child_overrides=verify_child_inheritance(mi,parent,slot+' after scalar update')
        local=list(mi.get_editor_property('scalar_parameter_values'))
        check('exactly one local scalar override',len(local)==1
              and str(local[0].get_editor_property('parameter_info').get_editor_property('name'))=='HeroLightMax'
              and abs(float(local[0].get_editor_property('parameter_value'))-.75)<1e-6)
        check('no local texture/vector overrides',not mi.get_editor_property('texture_parameter_values') and not mi.get_editor_property('vector_parameter_values'))
        A.set_metadata_tag(mi,KEY,OWNER);A.set_metadata_tag(mi,'SourceAsset',package(parent))
        check('save exact new child MI',A.save_loaded_asset(mi,False))
        saved=load(path)
        check('native saved MI effective parameters readback',instance_parameters(saved)==expected,dict(path=path,expected_max=.75))
        saved_overrides=verify_child_inheritance(saved,parent,slot+' after save')
        R['assets'].append(dict(path=path,sha256=sha(asset_file(path)),bytes=asset_file(path).stat().st_size,
                               source=package(parent),effective_parameters=expected,base_overrides=saved_overrides['export'],setter_return=setter))
        children.append(saved);dump()
    check('five new child assets only',len(children)==len(R['assets'])==5)
    bp.modify();cdo.modify();body.modify();body.set_editor_property('override_materials',children)
    check('native BP compile',U.BlueprintEditorLibrary.compile_blueprint(bp))
    bp,cdo,body=hero_objects();after=snapshot(cdo,body)
    check('regenerated CDO exact five child slots',after['materials']==after['override_materials']==[path_of(m) for m in children])
    check('all non-material CDO state unchanged',after['preserved']==before['preserved'] and after['slot_names']==before['slot_names'])
    check('only intended BP dirty before save',set(dirty()).issubset({TARGET}),dirty())
    ALLOWED.add(str(asset_file(TARGET)))
    check('native save only target BP',A.save_loaded_asset(bp,False))
    transition=dict(path=TARGET,before_sha256=BASELINE_SHA,after_sha256=sha(asset_file(TARGET)),backup=R['backup'],
                    before_native_cdo=before,after_native_cdo=after,materials=R['assets'].copy())
    R['transitions']=[transition];R['applied']=True;dump()
    check('changed serialized BP bytes',transition['after_sha256']!=BASELINE_SHA)
    bp,cdo,body=hero_objects();check('post-save native full CDO readback',snapshot(cdo,body)==after)
    check('no remaining dirty package',not dirty(),dirty())
    R['pass_scope']='Five child MIs saved, target BP compiled/saved and read back; separate-process Verify and real same-corner portraits still required.'


def prior_apply():
    matches=[]
    for file in (DOC/'editor_runtime').glob('author_*_HeroLightClampApply/hero_light_clamp_manifest.json'):
        m=json.loads(file.read_text('utf-8-sig'))
        if m.get('status')=='PASS' and m.get('schema')=='HarborCity.M5VS2.HeroLightClamp.v1' and len(m.get('transitions',[]))==1:
            if m['transitions'][0]['after_sha256']==sha(asset_file(TARGET)):matches.append((file,m))
    check('one successful Apply exactly matches current BP bytes',len(matches)==1,len(matches))
    file,m=matches[0];t=m['transitions'][0];p=Path(m['evidence']['path']).resolve()
    check('exact Apply evidence digest and phase',p==file.parent/'author_result.json' and sha(p)==m['evidence']['sha256'] and m['phase']=='HeroLightClampApply')
    a=json.loads(p.read_text('utf-8-sig'))
    check('successful Apply from another process',a['status']=='PASS' and a['applied'] is True and a['process_id']!=os.getpid()
          and a['transitions']==m['transitions'] and a['inputs']['script']['sha256']==sha(Path(__file__)))
    check('exact original baseline and private backup',t['path']==TARGET and t['before_sha256']==BASELINE_SHA
          and Path(t['backup']['path']).resolve().is_relative_to(BACKUPS) and sha(t['backup']['path'])==BASELINE_SHA)
    for f,h in a['protected_source_sha256_before'].items():protect(f,t['after_sha256'] if Path(f).resolve()==asset_file(TARGET) else h)
    for item in t['materials']:
        protect(asset_file(item['path']),item['sha256']);mi=load(item['path'])
        check('saved child source and full effective parameters',package(mi.get_editor_property('parent'))==item['source']
              and instance_parameters(mi)==item['effective_parameters']
              and mi.get_editor_property('base_property_overrides').export_text()==item['base_overrides'])
    R['inputs']['apply_manifest']=binding(file);R['inputs']['apply_report']=binding(p)
    R['transitions']=[t];R['backup']=t['backup']
    return t


def verify_or_restore(sources):
    t=prior_apply();bp,cdo,body=hero_objects();actual=snapshot(cdo,body)
    check('fresh process exact complete saved CDO',actual==t['after_native_cdo'])
    R['fresh_process_disk_readback']='PASS';R['native_cdo_readback']=actual
    if PHASE=='HeroLightClampRestore':
        bp.modify();cdo.modify();body.modify();body.set_editor_property('override_materials',sources)
        check('native restore BP compile',U.BlueprintEditorLibrary.compile_blueprint(bp))
        bp,cdo,body=hero_objects()
        check('restored original full native CDO',snapshot(cdo,body)==t['before_native_cdo'])
        check('only target BP dirty for restore',set(dirty()).issubset({TARGET}),dirty())
        ALLOWED.add(str(asset_file(TARGET)));check('native restore save target BP',A.save_loaded_asset(bp,False))
        bp,cdo,body=hero_objects();check('native saved restored original CDO',snapshot(cdo,body)==t['before_native_cdo'])
        R['restoration']=dict(status='PASS',method='NATIVE_ORIGINAL_FIVE_SLOT_SAVE_NOT_BYTE_OVERWRITE',
            restored_sha256=sha(asset_file(TARGET)),original_backup=t['backup'],candidates_retained=True)
        R['pass_scope']='Original Chroma050 five slots restored by native compile/save; original raw bytes and candidate evidence preserved.'
    else:R['pass_scope']='Fresh native process reload of exact five candidate MIs and full BP CDO; no save, no render/visual claim.'
    check('no remaining dirty package',not dirty(),dirty())


try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or initially dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False) and not dirty(),dirty())
    sources=source_preflight()
    for folder in ('Content/HarborCity/M5VS1','Content/HarborCity/M5VS2/HeroSelestia',
                   'Content/HarborCity/M5VS2/HeroLightChroma','Content/HarborCity/M5VS2/World',
                   'Content/HarborCity/M5VS2/NPC','Config'):
        tree(PROJECT/folder)
    R['protected_source_sha256_before']=dict(PROTECTED);dump()
    if PHASE=='HeroLightClampApply':apply(sources)
    else:verify_or_restore(sources)
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc()
finally:
    changed=[p for p,h in PROTECTED.items() if p not in ALLOWED and (not Path(p).is_file() or sha(p)!=h)]
    differences=[]
    for folder,before in TREES.items():
        after=sorted(str(p.resolve()) for p in Path(folder).rglob('*') if p.is_file())
        if after!=before:differences.append(dict(folder=folder,added=sorted(set(after)-set(before)),removed=sorted(set(before)-set(after))))
    R['preservation']=dict(files_checked=len(PROTECTED),allowed_changed_files=sorted(ALLOWED),unexpected_changed_files=changed,tree_membership_changes=differences)
    if changed or differences:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
    if R['status']=='PASS':
        manifest=dict(schema='HarborCity.M5VS2.HeroLightClamp.v1',status='PASS',phase=PHASE,
                      evidence=binding(OUT/'author_result.json'),transitions=R['transitions'],fresh_process_disk_readback=R['fresh_process_disk_readback'],
                      runtime='NOT_RUN',visual='USER_REVIEW')
        (OUT/'hero_light_clamp_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('Hero light-clamp author failed; retain exact failure and private backup, do not blindly repeat changed baseline.')
