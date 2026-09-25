"""Bounded read-only material graph/MPC probe; no compile, save, actor spawn or setters."""
from pathlib import Path
import hashlib, json, re, traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习');PROJECT=WORK/'HarborCity';DOCS=WORK/'docs/HarborCity_M5_VS2'
ROOT='/Game/HarborCity/M5VS2/HeroSelestia/Materials/MToon/'
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
assert len(args)==1,'One -M5EvidenceDir required'
OUT=Path(args[0][0] or args[0][1]).resolve();assert OUT.is_relative_to(DOCS.resolve())
OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'author_result.json').exists(),'Fresh evidence directory required'
E=U.MaterialEditingLibrary
R=dict(status='RUNNING',scope='READ_ONLY_GRAPH_BINDINGS_COLLECTION_DEFAULTS',asset_writes=0,
       shader_readiness='NOT_TESTED_BY_GRAPH_PROBE',runtime_world_values='NOT_TESTED_GAME_WORLD',
       instances={},graphs={},collections={},actor_blueprints={},errors=[],checks=[],limits=dict(graphs=160,nodes=10000,json_bytes=10*1024*1024))
protected={};queue=[];queued=set();total_nodes=0

def value(v):
    if v is None or isinstance(v,(str,int,float,bool)):return v
    if isinstance(v,U.LinearColor):return [v.r,v.g,v.b,v.a]
    if isinstance(v,U.Object):return v.get_path_name()
    if isinstance(v,(list,tuple)):return [value(x) for x in v]
    return str(v)
def prop(obj,name):
    try:return value(obj.get_editor_property(name))
    except Exception as ex:return {'unavailable':str(ex)[:300]}
def properties(obj,names):return {n:prop(obj,n) for n in names}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def disk(path):
    package=path.split('.')[0]
    if package.startswith('/VRM4U/'):return PROJECT/'Plugins/VRM4U/Content'/(package.removeprefix('/VRM4U/')+'.uasset')
    if package.startswith('/Game/'):return PROJECT/'Content'/(package.removeprefix('/Game/')+'.uasset')
    if package.startswith('/Engine/'):return Path('E:/UE_5.8/Engine/Content')/(package.removeprefix('/Engine/')+'.uasset')
    return None
def protect(obj_or_path):
    path=obj_or_path if isinstance(obj_or_path,str) else obj_or_path.get_path_name()
    p=disk(path)
    if p and p.exists() and str(p) not in protected:protected[str(p)]=sha(p)
def load(path):
    protect(path);obj=U.load_asset(path)
    if obj is None:raise RuntimeError('Missing asset: '+path)
    return obj
def dirty():
    return {k:sorted(p.get_path_name() for p in fn()) for k,fn in (
        ('maps',U.EditorLoadingAndSavingUtils.get_dirty_map_packages),('content',U.EditorLoadingAndSavingUtils.get_dirty_content_packages))}
def enqueue(obj):
    if not obj:return
    path=obj.get_path_name();protect(obj)
    if path not in queued:queue.append(obj);queued.add(path)
def instance_chain(obj):
    seen=set();chain=[]
    while isinstance(obj,U.MaterialInstanceConstant):
        path=obj.get_path_name();protect(obj)
        if path in seen:raise RuntimeError('Material parent cycle')
        seen.add(path);chain.append(path)
        if path not in R['instances']:
            row=dict(parent=prop(obj,'parent'),base_property_overrides=prop(obj,'base_property_overrides'),bindings={})
            for kind in ('scalar','vector','texture','static_switch'):
                names=getattr(E,'get_'+kind+'_parameter_names')(obj)
                if len(names)>512:raise RuntimeError('Unexpectedly unbounded parameter list')
                getter=getattr(E,'get_material_instance_'+kind+'_parameter_value')
                row['bindings'][kind]={str(n):value(getter(obj,n)) for n in names}
            R['instances'][path]=row
        obj=obj.get_editor_property('parent')
        if obj is None:raise RuntimeError('Null material parent')
        if len(chain)>12:raise RuntimeError('Material parent chain cap')
    if not isinstance(obj,U.Material):raise RuntimeError('Expected terminal UMaterial')
    enqueue(obj);return chain+[obj.get_path_name()]
def collection(obj):
    path=obj.get_path_name();protect(obj)
    if path in R['collections']:return
    row=dict(parameters={},referenced_by=[],runtime_instance_values='NOT_QUERIED_NO_GAME_WORLD')
    R['collections'][path]=row
    for kind in ('scalar','vector'):
        try:
            entries=obj.get_editor_property(kind+'_parameters')
            if len(entries)>256:raise RuntimeError('MPC parameter cap')
            row['parameters'][kind]=[properties(p,['parameter_name','default_value','id']) for p in entries]
        except Exception as ex:
            row['parameters'][kind]={'unavailable':str(ex)[:300]}
            R['errors'].append('MPC defaults unavailable '+path+' '+kind)
    getter=getattr(obj,'get_base_parameter_collection',None)
    row['base_collection']=value(getter()) if getter else 'API_UNAVAILABLE'
def inspect_graph(obj):
    global total_nodes
    path=obj.get_path_name();is_material=isinstance(obj,U.Material)
    if len(R['graphs'])>=R['limits']['graphs']:raise RuntimeError('Graph cap exceeded; no silent truncation')
    row={'kind':'material' if is_material else obj.get_class().get_name(),'nodes':[],'output_nodes':{}}
    R['graphs'][path]=row
    if is_material:
        row['properties']=properties(obj,['blend_mode','shading_model','two_sided','used_with_skeletal_mesh',
            'used_with_morph_targets','automatically_set_usage_in_editor','opacity_mask_clip_value',
            'use_material_attributes','translucency_lighting_mode'])
        nodes=E.get_material_expressions(obj)
        for n in ('MP_BASE_COLOR','MP_EMISSIVE_COLOR','MP_OPACITY','MP_OPACITY_MASK','MP_NORMAL',
                  'MP_WORLD_POSITION_OFFSET','MP_MATERIAL_ATTRIBUTES','MP_FRONT_MATERIAL'):
            enum=getattr(U.MaterialProperty,n,None)
            if enum is not None:
                try:row['output_nodes'][n]={'node':value(E.get_material_property_input_node(obj,enum)),
                    'output_name':E.get_material_property_input_node_output_name(obj,enum)}
                except Exception as ex:row['output_nodes'][n]={'unavailable':str(ex)[:200]}
    elif isinstance(obj,U.MaterialFunction):nodes=E.get_material_function_expressions(obj)
    else:
        row['unsupported_interface']=True
        row['parent']=prop(obj,'parent')
        try:enqueue(obj.get_editor_property('parent'))
        except Exception:pass
        R['errors'].append('Function interface graph unavailable '+path)
        return
    total_nodes+=len(nodes)
    if total_nodes>R['limits']['nodes']:raise RuntimeError('Node cap exceeded; no silent truncation')
    for n in nodes:
        kind=n.get_class().get_name();node={'id':n.get_name(),'class':kind}
        names=list(E.get_material_expression_input_names(n));outputs=list(E.get_material_expression_output_names(n))
        inputs=(E.get_inputs_for_material_expression(obj,n) if is_material else E.get_inputs_for_material_function_expression(obj,n))
        node['output_names']=[str(x) for x in outputs];node['inputs']=[]
        node['input_name_count']=len(names);node['input_link_count']=len(inputs)
        for index,upstream in enumerate(inputs):
            link={'index':index,'name':str(names[index]) if index<len(names) else None,'upstream':value(upstream)}
            if upstream:
                # Engine helper returns first matching source. Reused source output pins
                # may be ambiguous; retain that fact instead of inventing output indices.
                try:link['first_matching_source_output_name']=value(E.get_input_node_output_name_for_material_expression(n,upstream))
                except Exception as ex:link['output_name_unavailable']=str(ex)[:160]
                link['source_reused_in_inputs']=sum(x==upstream for x in inputs)>1
            node['inputs'].append(link)
        if kind=='MaterialExpressionMaterialFunctionCall':
            target=n.get_editor_property('material_function');node['material_function']=value(target);enqueue(target)
        elif kind=='MaterialExpressionCollectionParameter':
            target=n.get_editor_property('collection');node.update(properties(n,['parameter_name','parameter_id']))
            node['collection']=value(target)
            if target:
                collection(target);R['collections'][target.get_path_name()]['referenced_by'].append({'graph':path,'node':node['id'],'parameter':node['parameter_name']})
            else:R['errors'].append('Null MPC reference '+n.get_path_name())
        elif kind in ('MaterialExpressionFunctionInput','MaterialExpressionFunctionOutput'):
            node['properties']=properties(n,['input_name','input_type','preview_value','use_preview_value_as_default'] if kind.endswith('Input') else ['output_name','sort_priority'])
        elif 'Parameter' in kind:
            keys=['parameter_name']
            if 'Texture' in kind:keys+=['texture','sampler_type']
            elif 'StaticSwitch' in kind or 'Scalar' in kind or 'Vector' in kind:keys+=['default_value']
            node['properties']=properties(n,keys)
        elif kind=='MaterialExpressionCustom':
            code=n.get_editor_property('code');node['custom_code_sha256']=hashlib.sha256(code.encode('utf-8')).hexdigest()
            node['custom_code_chars']=len(code)
            node['code_mentions']=[s for s in ('EyeAdaptation','PreExposure','View.','SceneTexture','Light','Tonemap','Opacity') if s.lower() in code.lower()]
        elif kind=='MaterialExpressionConstant':node['constant']=prop(n,'r')
        elif kind in ('MaterialExpressionConstant3Vector','MaterialExpressionConstant4Vector'):node['constant']=prop(n,'constant')
        row['nodes'].append(node)

def actor_blueprint(path):
    bp=load(path);row={'path':bp.get_path_name(),'graphs':[],'candidate_cdo_values':{}}
    R['actor_blueprints'][path]=row
    try:
        registry=U.AssetRegistryHelpers.get_asset_registry()
        dependencies=registry.get_dependencies(path,U.AssetRegistryDependencyOptions())
        row['direct_package_dependencies']=[str(x) for x in dependencies]
        row['referenced_graph_collections']=[p for p in R['collections'] if p.split('.')[0] in row['direct_package_dependencies']]
    except Exception as ex:row['dependency_query_unavailable']=str(ex)[:300]
    try:
        cls=U.BlueprintEditorLibrary.generated_class(bp);cdo=U.get_default_object(cls)
        fields=[s for s in dir(cdo) if re.search('light|shadow|exposure|toon|gamma|saturat|collection',s,re.I) and not s.startswith('_')]
        for name in fields[:100]:
            try:
                v=cdo.get_editor_property(name)
                if not callable(v):row['candidate_cdo_values'][name]=value(v)
            except Exception:pass
    except Exception as ex:row['cdo_read_error']=str(ex)[:300]
    count=0
    for graph_type in ('ubergraph_pages','function_graphs'):
        try:graphs=bp.get_editor_property(graph_type)
        except Exception as ex:row[graph_type+'_unavailable']=str(ex)[:300];continue
        for graph in graphs:
            calls=[]
            for node in graph.get_editor_property('nodes'):
                count+=1
                if count>3000:raise RuntimeError('Bounded utility Blueprint graph cap exceeded')
                try:
                    ref=node.get_editor_property('function_reference')
                    calls.append({'node':node.get_name(),'function':properties(ref,['member_name','member_parent'])})
                except Exception:pass
            row['graphs'].append({'name':graph.get_name(),'function_calls':calls})
    row['execution']='NOT_EXECUTED_NO_ACTOR_CREATED'

before=dirty()
try:
    R['engine_version']=U.SystemLibrary.get_engine_version();R['dirty_before']=before
    R['parent_chains']={}
    for slot in ('hair','body','face','option','costume'):
        R['parent_chains'][slot]=instance_chain(load(ROOT+'MI_MToon_'+slot))
    enqueue(load('/VRM4U/MaterialUtil/UE5/Material/M_VrmMToonBaseOpaque'))
    while queue:inspect_graph(queue.pop(0))
    for path in ('/VRM4U/Util/Actor/MToonMaterialSystem','/VRM4U/Util/Actor/MToonAttachActor'):
        actor_blueprint(path)
    R['current_editor_scene']={'scope':'CURRENT_EDITOR_WORLD_ONLY_NOT_CAPTURE_GAME_WORLD','relevant_actors':[]}
    try:
        sub=U.get_editor_subsystem(U.EditorActorSubsystem)
        if sub:
            for actor in sub.get_all_level_actors():
                kind=actor.get_class().get_path_name()
                if re.search('MToon|DirectionalLight|SkyLight',kind,re.I):
                    R['current_editor_scene']['relevant_actors'].append({'path':actor.get_path_name(),'class':kind})
        else:R['current_editor_scene']['subsystem']='UNAVAILABLE'
    except Exception as ex:R['current_editor_scene']['unavailable']=str(ex)[:300]
    R['dirty_after']=dirty()
    if R['dirty_after']!=before:raise RuntimeError('Package dirty flags changed after read-only load')
    if any(sha(Path(p))!=h for p,h in protected.items()):raise RuntimeError('Protected graph bytes changed')
    R['checks']=[{'name':'loaded graph asset disk hashes unchanged','status':'PASS'},
        {'name':'dirty flags unchanged','status':'PASS'}]
    R['node_count']=total_nodes;R['status']='PASS'
    R['completeness']='COMPLETE_WITH_EXPLICIT_API_GAPS' if R['errors'] else 'GRAPH_TRAVERSAL_COMPLETE'
    R['pass_scope']='Read-only graph/parameter evidence only. Active static branches, GPU compile readiness and actual game MPC values are separate checks.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['loaded_asset_sha256']=protected
    data=json.dumps(R,ensure_ascii=False,indent=2)+'\n'
    if len(data.encode('utf-8'))>R['limits']['json_bytes']:
        R={'status':'FAIL','error':'Bounded graph output exceeded limit; no silent partial PASS','node_count':total_nodes}
        data=json.dumps(R,ensure_ascii=False,indent=2)+'\n'
    (OUT/'mtoon_graph_probe.json').write_text(data,encoding='utf-8')
    (OUT/'author_result.json').write_text(data,encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('MToon graph probe failed; inspect evidence')
