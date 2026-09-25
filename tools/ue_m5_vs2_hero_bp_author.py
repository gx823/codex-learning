"""Duplicate the accepted hero binding into the isolated VS2 namespace."""
from pathlib import Path
import json,re,hashlib,traceback
import unreal as U
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2'
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
OUT=Path(args[0][0] or args[0][1]).resolve();assert OUT.is_relative_to(D.resolve())
A=U.EditorAssetLibrary;ROOT='/Game/HarborCity/M5VS2/HeroSelestia';OLD='/Game/HarborCity/M5VS1/HeroSelestia';OWNER='HarborCity_M5_VS2_HeroBinding'
R={'status':'RUNNING','assets':[],'runtime':'NOT_RUN','visual':'NOT_RUN'}
def file(p):return W/'HarborCity/Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
try:
    pairs=[(OLD+'/BP_M5_Selestia',ROOT+'/BP_M5VS2_Selestia'),(OLD+'/BP_M5_SelestiaGameMode',ROOT+'/BP_M5VS2_SelestiaGameMode')]
    before={str(file(src)):sha(file(src)) for src,_ in pairs}
    created=[]
    for src,dst in pairs:
        if A.does_asset_exist(dst):
            obj=A.load_asset(dst);assert A.get_metadata_tag(obj,'HarborCityOwnedBy')==OWNER
        else:
            obj=A.duplicate_asset(src,dst);assert obj
            A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER)
        U.BlueprintEditorLibrary.compile_blueprint(obj)
        assert A.save_loaded_asset(obj,False)
        created.append(obj)
    hero_cls=U.BlueprintEditorLibrary.generated_class(created[0]);mode_cls=U.BlueprintEditorLibrary.generated_class(created[1])
    mode=U.get_default_object(mode_cls);mode.set_editor_property('default_pawn_class',hero_cls)
    U.BlueprintEditorLibrary.compile_blueprint(created[1]);assert A.save_loaded_asset(created[1],False)
    assert U.get_default_object(U.BlueprintEditorLibrary.generated_class(created[1])).get_editor_property('default_pawn_class')==hero_cls
    assert all(sha(Path(p))==h for p,h in before.items())
    R['assets']=[{'path':dst,'sha256':sha(file(dst))} for _,dst in pairs]
    R['status']='PASS';R['scope']='Only new Blueprint duplication and isolated GameMode binding; old gameplay and default map unchanged.'
except Exception:R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('VS2 hero BP author failed')
