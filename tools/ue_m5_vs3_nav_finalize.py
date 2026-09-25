from pathlib import Path
import unreal as U,json,re,traceback
ROOT=Path(__file__).resolve().parents[1];D=ROOT/'docs/HarborCity_M5_VS3'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);R={'status':'RUNNING'}
try:
 L=U.get_editor_subsystem(U.LevelEditorSubsystem);assert L.load_level(json.loads((D/'town_selection.json').read_text(encoding='utf-8-sig'))['map'])
 W=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
 assert U.HCM3EditorNavigation.build_for_authoring(W),'Nav build lock or failure'
 R['nav']=U.HCM4R2NavigationAuthor.inspect_navigation(W)
 A=U.EditorAssetLibrary;AT=U.AssetToolsHelpers.get_asset_tools()
 for i in range(5):
  name=f'SW_VS3_Magic{i}';dest='/Game/HarborCity/M5VS3/Combat'
  if not A.does_asset_exist(dest+'/'+name):
   t=U.AssetImportTask();t.filename=str(ROOT/'assets/M5_VS3/OriginalAudio'/(name+'.wav'));t.destination_path=dest;t.destination_name=name;t.automated=True;t.save=True;AT.import_asset_tasks([t]);assert A.does_asset_exist(dest+'/'+name)
 assert L.save_current_level();R['status']='PASS'
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
