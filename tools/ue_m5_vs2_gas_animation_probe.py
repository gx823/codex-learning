"""Read-only native inventory/selection probe in the completed official GAS 5.8 project.

Run only inside the actual downloaded .uproject below AUTHORIZED_SOURCE. It does
not mount VaultCache chunks, copy/migrate/save assets, edit graphs, or run gameplay.
Optional -M5GASSelection=<JSON> supplies exact discovered AnimSequence paths (<=32).
Without it, bounded name-hint candidates are inspected, never accepted as final.
"""
from pathlib import Path
import collections
import datetime
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
DOC = WORK / 'docs/HarborCity_M5_VS2'
AUTHORIZED_SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample').resolve()
DEST_CONTENT = WORK / 'HarborCity/Content'
AH = U.AssetRegistryHelpers
AR = AH.get_asset_registry()
A = U.EditorAssetLibrary
P = U.AnimPoseExtensions
L = U.AnimationLibrary
CMD = U.SystemLibrary.get_command_line()


def argument(name, required=False):
    values = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', CMD)
    if len(values) > 1 or (required and len(values) != 1):
        raise RuntimeError('Exactly one ' + name + ' required')
    return (values[0][0] or values[0][1]) if values else None


OUT = Path(argument('M5EvidenceDir', True)).resolve()
if not OUT.is_relative_to(DOC.resolve()) or (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh M5-VS2 evidence directory required')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         scope='READ_ONLY_SOURCE_ASSET_REGISTRY_AND_BOUNDED_RAW_POSE_INSPECTION',
         checks=[], inventory=[], candidate_inspections=[], skeleton_mesh_candidates={},
         migration='NOT_RUN', retarget='NOT_RUN', runtime='NOT_RUN', visual='NOT_RUN',
         selection_authority='Names are hints only. No source filename, count, style or semantic quality is assumed.',
         asset_writes=0, limitations=['Nine raw skeleton-pose samples are not a full trajectory or foot-contact audit.',
             'No chosen skeletal mesh is assumed from a shared Skeleton tag.',
             'Root/pelvis trajectories are source observations, not corrected target animations.',
             'No root-motion, animation flag, skeleton compatibility, socket or gameplay change.'])


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, success, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if success else 'FAIL', observed=observed))
    dump()
    if not success:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tag(asset, name):
    value = AH.get_tag_value(asset, U.Name(name))
    if isinstance(value, (tuple, list)) and len(value) == 2 and isinstance(value[0], bool):
        return str(value[1]) if value[0] else None
    return None if value is None else str(value)


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def transform(value):
    q = value.rotation
    return dict(translation_cm=vector(value.translation), quaternion_xyzw=[float(q.x), float(q.y), float(q.z), float(q.w)],
                scale=vector(value.scale3d))


def name_hints(name):
    # Tokenize actual asset names only; these are NOT accepted semantic classifications.
    words = set(re.findall(r'[a-z]+|\d+', re.sub(r'([a-z])([A-Z])', r'\1_\2', name).lower()))
    categories = []
    for category, tokens in [('walk', {'walk', 'walking'}), ('run', {'run', 'running', 'jog', 'sprint'}),
            ('start', {'start', 'starts'}), ('stop', {'stop', 'stops'}), ('turn', {'turn', 'turns', 'pivot'}),
            ('jump', {'jump', 'fall', 'land', 'airborne'}), ('idle_micro', {'idle', 'fidget', 'relaxed'})]:
        if words & tokens:
            categories.append(category)
    return categories


def inspect_sequence(asset, source_root):
    package = str(asset.package_name)
    path = source_root / 'Content' / Path(package.removeprefix('/Game/')).with_suffix('.uasset')
    row = dict(path=package, status='RUNNING', source_sha256_before=sha(path), source_bytes=path.stat().st_size)
    R['candidate_inspections'].append(row)
    try:
        sequence = AH.get_asset(asset)
        if not isinstance(sequence, U.AnimSequence):
            raise RuntimeError('Selected registry asset is not native AnimSequence')
        skeleton = sequence.get_editor_property('skeleton')
        if not skeleton:
            raise RuntimeError('Animation has no Skeleton')
        duration = float(L.get_sequence_length(sequence))
        if not math.isfinite(duration) or duration <= 0 or duration > 120:
            raise RuntimeError('Bounded inspection needs duration (0,120] seconds')
        row.update(skeleton=skeleton.get_path_name(), duration_seconds=duration,
                   sampled_keys=int(L.get_num_keys(sequence)), sampled_frames=int(L.get_num_frames(sequence)),
                   rate_scale=float(L.get_rate_scale(sequence)),
                   additive_type=str(sequence.get_editor_property('additive_anim_type')),
                   additive_base_type=str(sequence.get_editor_property('ref_pose_type')),
                   additive_base_frame=int(sequence.get_editor_property('ref_frame_index')),
                   enable_root_motion=bool(sequence.get_editor_property('enable_root_motion')),
                   force_root_lock=bool(sequence.get_editor_property('force_root_lock')),
                   root_motion_root_lock=str(sequence.get_editor_property('root_motion_root_lock')))
        base = sequence.get_editor_property('ref_pose_seq')
        row['additive_base_sequence'] = base.get_path_name() if base else None
        row['track_names'] = [str(x) for x in L.get_animation_track_names(sequence)]
        row['float_curve_names'] = [str(x) for x in L.get_animation_curve_names(sequence, U.RawCurveTrackTypes.RCT_FLOAT)]
        options = U.AnimPoseEvaluationOptions()
        for key, value in dict(evaluation_type=U.AnimDataEvalType.RAW, should_retarget=True,
                extract_root_motion=False, incorporate_root_motion_into_pose=True, retrieve_additive_as_full_pose=True).items():
            options.set_editor_property(key, value)
        first_pose = P.get_anim_pose_at_time(sequence, 0., options)
        if not P.is_valid(first_pose):
            raise RuntimeError('Native raw skeleton pose invalid')
        names = [str(x) for x in P.get_bone_names(first_pose)]
        if not names:
            raise RuntimeError('No evaluated bones')
        # FindBonePathToRoot's final entry is the actual Skeleton root (verified in local engine source).
        if not row['track_names']:
            raise RuntimeError('No native source tracks to resolve a raw skeleton root')
        # Pose names may include virtual bones; a real data-model track is a raw
        # bone and avoids FindBonePathToRoot's virtual-name fallback.
        chain = [str(x) for x in L.find_bone_path_to_root(sequence, U.Name(row['track_names'][-1]))]
        if not chain:
            raise RuntimeError('Could not resolve actual root chain')
        root_bone = chain[-1]
        pelvis_names = [x for x in names if x.casefold() in ('pelvis', 'hips')]
        row['reference_bone_names'] = names
        row['root_bone'] = root_bone
        row['pelvis_name_candidates'] = pelvis_names
        row['pose_mesh_basis'] = 'Skeleton/reference retarget basis; optional source mesh NOT_SELECTED'
        sample_bones = list(dict.fromkeys([root_bone] + pelvis_names))
        samples = []
        for index in range(9):
            time = duration * index / 8.
            pose = P.get_anim_pose_at_time(sequence, time, options)
            if not P.is_valid(pose):
                raise RuntimeError('Invalid raw pose at time ' + str(time))
            samples.append(dict(time_seconds=time, bones={bone: transform(P.get_bone_pose(pose, U.Name(bone), U.AnimPoseSpaces.WORLD)) for bone in sample_bones}))
        row['nine_raw_component_samples'] = samples
        start = samples[0]['bones'][root_bone]['translation_cm']
        row['root_xy_max_displacement_from_start_cm_9_samples'] = max(math.hypot(s['bones'][root_bone]['translation_cm'][0]-start[0],
                  s['bones'][root_bone]['translation_cm'][1]-start[1]) for s in samples)
        row['source_sha256_after'] = sha(path)
        if row['source_sha256_after'] != row['source_sha256_before']:
            raise RuntimeError('Read-only source bytes changed')
        row['status'] = 'PASS'
    except Exception:
        row['status'] = 'FAIL'
        row['error'] = traceback.format_exc()
    dump()


def main():
    source = Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()
    check('actual source project lies under authorized completed GAS directory', source.is_relative_to(AUTHORIZED_SOURCE), str(source))
    projects = list(source.glob('*.uproject'))
    check('one actual source uproject exists', len(projects) == 1, [str(p) for p in projects])
    check('actual engine is 5.8', U.SystemLibrary.get_engine_version().startswith('5.8'), U.SystemLibrary.get_engine_version())
    check('source Content exists', (source / 'Content').is_dir())
    R['source_project'] = dict(path=str(projects[0]), sha256=sha(projects[0]), engine=U.SystemLibrary.get_engine_version())
    initial_dirty = [x.get_path_name() for x in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    AR.scan_paths_synchronous(['/Game'], force_rescan=True)
    assets = list(AR.get_assets_by_path('/Game', recursive=True, include_only_on_disk_assets=True))
    check('bounded actual on-disk registry inventory', 0 < len(assets) <= 20000, len(assets))
    R['actual_registry_asset_count'] = len(assets)
    classes = collections.Counter(str(x.asset_class_path.asset_name) for x in assets)
    R['actual_class_counts'] = dict(sorted(classes.items()))
    relevant = {'AnimSequence', 'Skeleton', 'SkeletalMesh', 'IKRigDefinition', 'IKRetargeter', 'PoseSearchDatabase', 'PoseSearchSchema'}
    sequences = {}
    for asset in sorted(assets, key=lambda a: str(a.package_name)):
        cls = str(asset.asset_class_path.asset_name)
        if cls not in relevant:
            continue
        package = str(asset.package_name)
        row = dict(path=package, asset_name=str(asset.asset_name), class_name=cls,
            tags={k: tag(asset, k) for k in ('Skeleton','SequenceLength','Source Frame Rate','Target Frame Rate',
                    'Number of Frames','Number of Keys','AdditiveAnimType','bEnableRootMotion','RetargetSource','RetargetSourceAsset')})
        if cls == 'AnimSequence':
            row['name_hints_only'] = name_hints(str(asset.asset_name))
            sequences[package] = asset
        if cls == 'SkeletalMesh':
            R['skeleton_mesh_candidates'].setdefault(row['tags']['Skeleton'] or 'TAG_UNKNOWN', []).append(package)
        R['inventory'].append(row)
    check('actual source AnimSequence assets found', bool(sequences), len(sequences))
    requested = argument('M5GASSelection')
    if requested:
        selection_path = Path(requested).resolve()
        check('selection metadata stays in current reports', selection_path.is_relative_to(DOC.resolve()) and selection_path.is_file())
        selected = json.loads(selection_path.read_text('utf-8-sig'))['sequence_paths']
        R['selection_file'] = dict(path=str(selection_path), sha256=sha(selection_path))
    else:
        selected = []
        for category in ('idle_micro','walk','run','start','stop','turn','jump'):
            hints = [r['path'] for r in R['inventory'] if category in r.get('name_hints_only', [])]
            R.setdefault('name_hint_counts', {})[category] = len(hints)
            selected += hints[:3]
        selected = list(dict.fromkeys(selected))
    check('at most 32 exact registered source sequence paths', isinstance(selected, list) and len(selected) <= 32
          and len(set(selected)) == len(selected) and all(p in sequences for p in selected), selected)
    R['inspected_paths'] = selected
    for path in selected:
        inspect_sequence(sequences[path], source)
    # Dependency preview only. A later explicit migration must collision-check
    # every package before calling the real dependency-aware AssetTools migration.
    opts = U.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
    queue, visited, external = list(selected), set(), set()
    while queue and len(visited) < 1000:
        package = str(queue.pop())
        if package in visited:
            continue
        if not package.startswith('/Game/'):
            external.add(package)
            continue
        visited.add(package)
        queue.extend(str(x) for x in AR.get_dependencies(package, opts))
    R['migration_dependency_preview'] = dict(status='TRUNCATED_REQUIRES_NARROWER_SELECTION' if queue else 'COMPLETE_METADATA_ONLY',
        package_paths=sorted(visited), external_packages=sorted(external), unresolved_queue_count=len(queue),
        existing_destination_collisions=[p for p in sorted(visited) if (DEST_CONTENT / Path(p.removeprefix('/Game/')).with_suffix('.uasset')).exists()],
        includes_framework_assets=[str(a.package_name) for a in assets if str(a.package_name) in visited and str(a.asset_class_path.asset_name)
                                   in ('AnimBlueprint','Blueprint','World','PoseSearchDatabase','PoseSearchSchema')])
    R['source_dirty_packages_before'] = initial_dirty
    R['source_dirty_packages_after'] = [x.get_path_name() for x in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    R['read_only_note'] = 'No save/migrate/duplicate/controller mutation API called. Loading may leave transient dirty packages; none are saved.'
    check('source uproject bytes unchanged', sha(projects[0]) == R['source_project']['sha256'])
    check('every bounded candidate inspection completed', all(x['status'] == 'PASS' for x in R['candidate_inspections']))
    R['status'] = 'PASS'
    R['scope_result'] = 'Inventory and bounded raw pose reads only. Empty semantic hints mean candidate selection remains NOT_RUN.'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['ended_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('GAS source probe failed; see preserved author_result.json')
