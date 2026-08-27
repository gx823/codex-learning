'use client';

import { type ChangeEvent, useCallback, useEffect, useRef, useState } from 'react';

type GameStatus = 'menu' | 'playing' | 'paused' | 'levelclear' | 'gameover' | 'victory';
type GameMode = 'campaign' | 'endless';
type ObjectKind = 'letter' | 'cloud' | 'heart';
type CharacterId = 'mio' | 'elaina' | 'frieren';

type FlyingObject = {
  id: number;
  kind: ObjectKind;
  x: number;
  y: number;
  vx: number;
  size: number;
  spin: number;
};

type Particle = {
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  maxLife: number;
  color: string;
  size: number;
};

type GameModel = {
  width: number;
  height: number;
  player: { x: number; y: number; vx: number; vy: number };
  objects: FlyingObject[];
  particles: Particle[];
  stars: { x: number; y: number; size: number; speed: number; alpha: number }[];
  score: number;
  combo: number;
  energy: number;
  lives: number;
  character: CharacterId;
  mode: GameMode;
  level: number;
  delivered: number;
  totalDelivered: number;
  time: number;
  spawnIn: number;
  dashTime: number;
  invincible: number;
  worldTime: number;
  nextId: number;
  lastUiUpdate: number;
};

const PRE_FINAL_TARGETS = [8, 12, 16, 20, 24, 28, 32] as const;
const FINAL_LEVEL_TARGET = PRE_FINAL_TARGETS.reduce((sum, target) => sum + target, 0);
const ENDLESS_THRESHOLDS = [0, 15, 30, 45, 60, 75, 90, 120] as const;

const CHARACTERS: Record<CharacterId, {
  name: string;
  title: string;
  perk: string;
  sprite: string;
  maxLives: number;
  moveSpeed: number;
  letterValue: number;
  magnetRadius: number;
}> = {
  mio: {
    name: '星野澪',
    title: '均衡型 · 夜行魔女',
    perk: '生命 3 · 标准速度',
    sprite: '/stellar-courier.png',
    maxLives: 3,
    moveSpeed: 1,
    letterValue: 1,
    magnetRadius: 230,
  },
  elaina: {
    name: '伊雷娜',
    title: '耐久型 · 灰之魔女',
    perk: '生命 5 · 移速 82%',
    sprite: '/witch-silver.png',
    maxLives: 5,
    moveSpeed: 0.82,
    letterValue: 1,
    magnetRadius: 230,
  },
  frieren: {
    name: '芙莉莲',
    title: '收集型 · 白袍法师',
    perk: '生命 2 · 信封 ×2',
    sprite: '/witch-white-mage.png',
    maxLives: 2,
    moveSpeed: 1.08,
    letterValue: 2,
    magnetRadius: 310,
  },
};

const CHARACTER_IDS = Object.keys(CHARACTERS) as CharacterId[];

const BACKGROUND_THEMES = [
  { top: '#625da0', middle: '#9c90cf', bottom: '#f1b7c6', celestial: '#fff0ba', silhouette: '#464263', light: '#ffd86c' },
  { top: '#352a72', middle: '#7259a6', bottom: '#c79bc9', celestial: '#e8d8ff', silhouette: '#30294f', light: '#d9b5ff' },
  { top: '#29356f', middle: '#645da2', bottom: '#ec9fae', celestial: '#ffe6ed', silhouette: '#353556', light: '#ff9fb3' },
  { top: '#123f63', middle: '#30718a', bottom: '#8ec5c2', celestial: '#dffcf6', silhouette: '#173c50', light: '#7af3dd' },
  { top: '#17345f', middle: '#3c7390', bottom: '#b9dae2', celestial: '#e8ffff', silhouette: '#284f68', light: '#85ffd2' },
  { top: '#281f57', middle: '#674674', bottom: '#d16b78', celestial: '#fff1c4', silhouette: '#352341', light: '#ffb45f' },
  { top: '#160f35', middle: '#38214d', bottom: '#8e4552', celestial: '#ff8b78', silhouette: '#1d1730', light: '#ff7267' },
  { top: '#100c1d', middle: '#401526', bottom: '#b33b29', celestial: '#ff633e', silhouette: '#160f1b', light: '#ffb027' },
] as const;

const LEVELS = [
  { name: '黄昏屋檐', subtitle: '让扫帚适应晚风', target: PRE_FINAL_TARGETS[0], time: 34, speed: 0.86, cloudSpeed: 0.9, cloudRate: 0.22, spawnBase: 0.82 },
  { name: '紫藤钟楼', subtitle: '钟声会惊醒雨云', target: PRE_FINAL_TARGETS[1], time: 38, speed: 1.02, cloudSpeed: 1.05, cloudRate: 0.3, spawnBase: 0.7 },
  { name: '银河回廊', subtitle: '穿过最拥挤的星路', target: PRE_FINAL_TARGETS[2], time: 42, speed: 1.18, cloudSpeed: 1.2, cloudRate: 0.36, spawnBase: 0.6 },
  { name: '黎明天际', subtitle: '赶在第一缕晨光之前', target: PRE_FINAL_TARGETS[3], time: 46, speed: 1.36, cloudSpeed: 1.35, cloudRate: 0.41, spawnBase: 0.5 },
  { name: '寂夜浮岛', subtitle: '漂浮岛屿遮住了航标', target: PRE_FINAL_TARGETS[4], time: 54, speed: 1.48, cloudSpeed: 1.55, cloudRate: 0.44, spawnBase: 0.44 },
  { name: '极光裂谷', subtitle: '在变幻的极光间穿行', target: PRE_FINAL_TARGETS[5], time: 60, speed: 1.6, cloudSpeed: 1.75, cloudRate: 0.46, spawnBase: 0.4 },
  { name: '天穹禁区', subtitle: '风暴封锁的最后航道', target: PRE_FINAL_TARGETS[6], time: 68, speed: 1.72, cloudSpeed: 2, cloudRate: 0.48, spawnBase: 0.36 },
  { name: '究极无敌地狱关卡', subtitle: '所有夜航考验在此汇聚', target: FINAL_LEVEL_TARGET, time: 190, speed: 1.9, cloudSpeed: 2.8, cloudRate: 0.48, spawnBase: 0.22 },
] as const;

type UiState = {
  score: number;
  combo: number;
  energy: number;
  lives: number;
  mode: GameMode;
  level: number;
  delivered: number;
  target: number;
  time: number;
};

function getEndlessLevel(delivered: number) {
  for (let index = ENDLESS_THRESHOLDS.length - 1; index >= 0; index -= 1) {
    if (delivered >= ENDLESS_THRESHOLDS[index]) return index;
  }
  return 0;
}

function createModel(mode: GameMode = 'campaign', character: CharacterId = 'mio'): GameModel {
  return {
    width: 900,
    height: 520,
    player: { x: 100, y: 240, vx: 0, vy: 0 },
    objects: [],
    particles: [],
    stars: Array.from({ length: 68 }, (_, index) => ({
      x: (index * 137.7) % 1000,
      y: (index * 83.3) % 600,
      size: 0.7 + (index % 4) * 0.45,
      speed: 8 + (index % 5) * 8,
      alpha: 0.35 + (index % 6) * 0.1,
    })),
    score: 0,
    combo: 0,
    energy: 0,
    lives: CHARACTERS[character].maxLives,
    character,
    mode,
    level: 0,
    delivered: 0,
    totalDelivered: 0,
    time: mode === 'endless' ? Number.POSITIVE_INFINITY : LEVELS[0].time,
    spawnIn: 0.7,
    dashTime: 0,
    invincible: 0,
    worldTime: 0,
    nextId: 1,
    lastUiUpdate: 0,
  };
}

const INITIAL_UI: UiState = {
  score: 0,
  combo: 0,
  energy: 0,
  lives: 3,
  mode: 'campaign',
  level: 0,
  delivered: 0,
  target: LEVELS[0].target,
  time: LEVELS[0].time,
};

export default function Home() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const stageRef = useRef<HTMLDivElement>(null);
  const modelRef = useRef<GameModel>(createModel());
  const statusRef = useRef<GameStatus>('menu');
  const keysRef = useRef<Set<string>>(new Set());
  const pointerRef = useRef<{ active: boolean; x: number; y: number }>({ active: false, x: 0, y: 0 });
  const characterImagesRef = useRef<Partial<Record<CharacterId, HTMLImageElement>>>({});
  const audioRef = useRef<AudioContext | null>(null);
  const bgmRef = useRef<HTMLAudioElement | null>(null);
  const customBgmUrlRef = useRef<string | null>(null);
  const generatedBgmRef = useRef<{ timer: number | null; master: GainNode | null; step: number }>({ timer: null, master: null, step: 0 });
  const mutedRef = useRef(false);
  const [status, setStatus] = useState<GameStatus>('menu');
  const [ui, setUi] = useState<UiState>(INITIAL_UI);
  const [muted, setMuted] = useState(false);
  const [highScore, setHighScore] = useState(0);
  const [isShareVersion, setIsShareVersion] = useState(false);
  const [customMusicName, setCustomMusicName] = useState('');
  const [selectedCharacter, setSelectedCharacter] = useState<CharacterId>('mio');

  const setGameStatus = useCallback((next: GameStatus) => {
    statusRef.current = next;
    setStatus(next);
  }, []);

  const ensureAudio = useCallback(() => {
    if (audioRef.current) return audioRef.current;
    const AudioCtor = window.AudioContext || (window as typeof window & { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!AudioCtor) return null;
    audioRef.current = new AudioCtor();
    return audioRef.current;
  }, []);

  const playSound = useCallback((kind: 'catch' | 'hit' | 'dash' | 'start' | 'end') => {
    if (mutedRef.current) return;
    const audio = ensureAudio();
    if (!audio) return;
    if (audio.state === 'suspended') void audio.resume();
    const notes = {
      catch: [720, 980],
      hit: [170, 110],
      dash: [440, 660, 920],
      start: [330, 494, 659],
      end: [523, 392, 330],
    }[kind];
    notes.forEach((frequency, index) => {
      const oscillator = audio.createOscillator();
      const gain = audio.createGain();
      oscillator.type = kind === 'hit' ? 'sawtooth' : 'sine';
      oscillator.frequency.setValueAtTime(frequency, audio.currentTime + index * 0.07);
      gain.gain.setValueAtTime(0.0001, audio.currentTime + index * 0.07);
      gain.gain.exponentialRampToValueAtTime(kind === 'hit' ? 0.055 : 0.035, audio.currentTime + index * 0.07 + 0.012);
      gain.gain.exponentialRampToValueAtTime(0.0001, audio.currentTime + index * 0.07 + 0.18);
      oscillator.connect(gain).connect(audio.destination);
      oscillator.start(audio.currentTime + index * 0.07);
      oscillator.stop(audio.currentTime + index * 0.07 + 0.2);
    });
  }, [ensureAudio]);

  const stopBgm = useCallback(() => {
    bgmRef.current?.pause();
    const generated = generatedBgmRef.current;
    if (generated.timer !== null) window.clearInterval(generated.timer);
    generated.master?.disconnect();
    generatedBgmRef.current = { timer: null, master: null, step: generated.step };
  }, []);

  const startBgm = useCallback((restart = false) => {
    if (mutedRef.current) return;
    const isLocalGame = window.location.hostname === '127.0.0.1' || window.location.hostname === 'localhost';
    let track = bgmRef.current;
    if (isLocalGame || track) {
      if (!track && isLocalGame) {
        track = new Audio('/audio/styx-helix.mp3');
        track.loop = true;
        track.preload = 'auto';
        track.volume = 0.38;
        bgmRef.current = track;
      }
      if (!track) return;
      track.loop = true;
      track.defaultPlaybackRate = 1;
      track.playbackRate = 1;
      if (restart) track.currentTime = 0;
      void track.play().catch(() => undefined);
      return;
    }

    stopBgm();
    const audio = ensureAudio();
    if (!audio) return;
    if (audio.state === 'suspended') void audio.resume();
    const master = audio.createGain();
    master.gain.setValueAtTime(0.58, audio.currentTime);
    master.connect(audio.destination);
    generatedBgmRef.current.master = master;
    if (restart) generatedBgmRef.current.step = 0;
    const melody: Array<number | null> = [
      659.25, 783.99, 880, 783.99, 659.25, 587.33, 659.25, null,
      523.25, 659.25, 783.99, 880, 783.99, 659.25, 587.33, null,
    ];
    const bass = [164.81, 130.81, 146.83, 196];
    const playNote = (frequency: number, type: OscillatorType, time: number, duration: number, volume: number) => {
      const oscillator = audio.createOscillator();
      const gain = audio.createGain();
      oscillator.type = type;
      oscillator.frequency.setValueAtTime(frequency, time);
      gain.gain.setValueAtTime(0.0001, time);
      gain.gain.exponentialRampToValueAtTime(volume, time + 0.02);
      gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);
      oscillator.connect(gain).connect(master);
      oscillator.start(time);
      oscillator.stop(time + duration + 0.03);
    };
    const schedule = () => {
      if (mutedRef.current || statusRef.current !== 'playing') return;
      const step = generatedBgmRef.current.step;
      const time = audio.currentTime + 0.035;
      const melodyNote = melody[step % melody.length];
      if (melodyNote) playNote(melodyNote, 'sine', time, 0.2, 0.026);
      if (step % 2 === 0) playNote(bass[Math.floor(step / 4) % bass.length], 'triangle', time, 0.38, 0.018);
      generatedBgmRef.current.step = (step + 1) % melody.length;
    };
    schedule();
    generatedBgmRef.current.timer = window.setInterval(schedule, 225);
  }, [ensureAudio, stopBgm]);

  const chooseCustomBgm = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;
    stopBgm();
    if (customBgmUrlRef.current) URL.revokeObjectURL(customBgmUrlRef.current);
    const objectUrl = URL.createObjectURL(file);
    customBgmUrlRef.current = objectUrl;
    const track = new Audio(objectUrl);
    track.loop = true;
    track.preload = 'auto';
    track.volume = 0.38;
    track.defaultPlaybackRate = 1;
    track.playbackRate = 1;
    bgmRef.current = track;
    mutedRef.current = false;
    setMuted(false);
    setCustomMusicName(file.name.replace(/\.mp3$/i, ''));
    if (statusRef.current !== 'paused') void track.play().catch(() => undefined);
    event.target.value = '';
  }, [stopBgm]);

  const chooseCharacter = useCallback((character: CharacterId) => {
    setSelectedCharacter(character);
    modelRef.current.character = character;
    modelRef.current.lives = CHARACTERS[character].maxLives;
    setUi((previous) => ({ ...previous, lives: CHARACTERS[character].maxLives }));
    window.localStorage.setItem('starry-post-character', character);
  }, []);

  const activateDash = useCallback(() => {
    const model = modelRef.current;
    if (statusRef.current !== 'playing' || model.energy < 100 || model.dashTime > 0) return;
    model.energy = 0;
    model.dashTime = 3.2;
    model.invincible = 3.2;
    playSound('dash');
    setUi((previous) => ({ ...previous, energy: 0 }));
  }, [playSound]);

  const startGame = useCallback((mode: GameMode = 'campaign') => {
    const current = modelRef.current;
    const next = createModel(mode, selectedCharacter);
    next.width = current.width;
    next.height = current.height;
    next.player = { x: Math.max(36, next.width * 0.11), y: Math.max(80, next.height * 0.47), vx: 0, vy: 0 };
    Object.assign(current, next);
    modelRef.current = current;
    pointerRef.current.active = false;
    keysRef.current.clear();
    setUi({
      ...INITIAL_UI,
      lives: next.lives,
      mode,
      time: mode === 'endless' ? Number.POSITIVE_INFINITY : LEVELS[0].time,
      target: mode === 'endless' ? ENDLESS_THRESHOLDS[1] : LEVELS[0].target,
    });
    setGameStatus('playing');
    playSound('start');
    startBgm(true);
  }, [playSound, selectedCharacter, setGameStatus, startBgm]);

  const startNextLevel = useCallback(() => {
    const model = modelRef.current;
    const nextLevel = Math.min(LEVELS.length - 1, model.level + 1);
    const config = LEVELS[nextLevel];
    model.level = nextLevel;
    model.delivered = 0;
    model.time = config.time;
    model.spawnIn = 0.75;
    model.objects = [];
    model.particles = [];
    model.combo = 0;
    model.dashTime = 0;
    model.invincible = 1.2;
    model.player.x = Math.max(36, model.width * 0.11);
    model.player.y = Math.max(80, model.height * 0.47);
    setUi({
      score: Math.round(model.score),
      combo: 0,
      energy: Math.round(model.energy),
      lives: model.lives,
      mode: model.mode,
      level: nextLevel,
      delivered: 0,
      target: config.target,
      time: config.time,
    });
    setGameStatus('playing');
    playSound('start');
    startBgm();
  }, [playSound, setGameStatus, startBgm]);

  const togglePause = useCallback(() => {
    if (statusRef.current === 'playing') {
      stopBgm();
      setGameStatus('paused');
    } else if (statusRef.current === 'paused') {
      setGameStatus('playing');
      startBgm();
    }
  }, [setGameStatus, startBgm, stopBgm]);

  const resumeAndDash = useCallback(() => {
    if (statusRef.current === 'paused') {
      setGameStatus('playing');
      startBgm();
    }
    activateDash();
  }, [activateDash, setGameStatus, startBgm]);

  useEffect(() => {
    const saved = Number(window.localStorage.getItem('starry-post-high-score') || 0);
    setHighScore(Number.isFinite(saved) ? saved : 0);
    setIsShareVersion(window.location.hostname !== '127.0.0.1' && window.location.hostname !== 'localhost');
    const savedCharacter = window.localStorage.getItem('starry-post-character') as CharacterId | null;
    if (savedCharacter && CHARACTER_IDS.includes(savedCharacter)) {
      setSelectedCharacter(savedCharacter);
      modelRef.current.character = savedCharacter;
      modelRef.current.lives = CHARACTERS[savedCharacter].maxLives;
      setUi((previous) => ({ ...previous, lives: CHARACTERS[savedCharacter].maxLives }));
    }
    CHARACTER_IDS.forEach((characterId) => {
      const image = new Image();
      image.src = CHARACTERS[characterId].sprite;
      characterImagesRef.current[characterId] = image;
    });
    return () => {
      stopBgm();
      if (customBgmUrlRef.current) URL.revokeObjectURL(customBgmUrlRef.current);
    };
  }, [stopBgm]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const key = event.key.toLowerCase();
      if (['arrowup', 'arrowdown', 'arrowleft', 'arrowright', ' ', 'w', 'a', 's', 'd', 'p'].includes(key)) event.preventDefault();
      if (key === ' ' && statusRef.current === 'menu') startGame();
      else if (key === ' ' && (statusRef.current === 'gameover' || statusRef.current === 'victory')) startGame();
      else if (key === ' ' && statusRef.current === 'levelclear') startNextLevel();
      else if (key === ' ') resumeAndDash();
      else if (key === 'p') togglePause();
      keysRef.current.add(key);
    };
    const onKeyUp = (event: KeyboardEvent) => keysRef.current.delete(event.key.toLowerCase());
    const onBlur = () => {
      keysRef.current.clear();
      pointerRef.current.active = false;
      if (statusRef.current === 'playing') {
        stopBgm();
        setGameStatus('paused');
      }
    };
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('blur', onBlur);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
    };
  }, [resumeAndDash, setGameStatus, startGame, startNextLevel, stopBgm, togglePause]);

  useEffect(() => {
    const canvas = canvasRef.current;
    const stage = stageRef.current;
    if (!canvas || !stage) return;
    const context = canvas.getContext('2d');
    if (!context) return;
    const model = modelRef.current;

    const resize = () => {
      const bounds = stage.getBoundingClientRect();
      const ratio = Math.min(window.devicePixelRatio || 1, 2);
      model.width = Math.max(320, bounds.width);
      model.height = Math.max(420, bounds.height);
      canvas.width = Math.round(model.width * ratio);
      canvas.height = Math.round(model.height * ratio);
      canvas.style.width = `${model.width}px`;
      canvas.style.height = `${model.height}px`;
      context.setTransform(ratio, 0, 0, ratio, 0, 0);
    };
    resize();
    const observer = new ResizeObserver(resize);
    observer.observe(stage);

    const roundedRect = (ctx: CanvasRenderingContext2D, x: number, y: number, width: number, height: number, radius: number) => {
      ctx.beginPath();
      ctx.roundRect(x, y, width, height, radius);
    };

    const burst = (x: number, y: number, color: string, count: number) => {
      for (let index = 0; index < count; index += 1) {
        const angle = Math.random() * Math.PI * 2;
        const speed = 45 + Math.random() * 110;
        model.particles.push({
          x,
          y,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed,
          life: 0.45 + Math.random() * 0.45,
          maxLife: 0.9,
          color,
          size: 2 + Math.random() * 4,
        });
      }
    };

    const drawEnvelope = (item: FlyingObject) => {
      context.save();
      context.translate(item.x, item.y);
      context.rotate(item.spin);
      context.shadowColor = '#fff2a8';
      context.shadowBlur = 22;
      context.fillStyle = '#fff9d9';
      context.strokeStyle = '#554f87';
      context.lineWidth = 2;
      roundedRect(context, -item.size * 0.55, -item.size * 0.36, item.size * 1.1, item.size * 0.72, 6);
      context.fill();
      context.shadowBlur = 0;
      context.stroke();
      context.beginPath();
      context.moveTo(-item.size * 0.52, -item.size * 0.31);
      context.lineTo(0, item.size * 0.08);
      context.lineTo(item.size * 0.52, -item.size * 0.31);
      context.strokeStyle = '#e49a9e';
      context.stroke();
      context.fillStyle = '#f3a0b9';
      context.font = `bold ${item.size * 0.4}px serif`;
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.fillText('✦', 0, item.size * 0.13);
      context.restore();
    };

    const drawCloud = (item: FlyingObject) => {
      context.save();
      context.translate(item.x, item.y);
      context.rotate(Math.sin(model.worldTime * 2 + item.id) * 0.04);
      context.fillStyle = '#625e8c';
      context.strokeStyle = '#383552';
      context.lineWidth = 2;
      context.beginPath();
      context.arc(-item.size * 0.2, 0, item.size * 0.27, Math.PI, 0);
      context.arc(item.size * 0.06, -item.size * 0.12, item.size * 0.34, Math.PI, 0);
      context.arc(item.size * 0.37, 0, item.size * 0.25, Math.PI, 0);
      context.lineTo(item.size * 0.55, item.size * 0.2);
      context.lineTo(-item.size * 0.46, item.size * 0.2);
      context.closePath();
      context.fill();
      context.stroke();
      context.fillStyle = '#ffd45f';
      context.beginPath();
      context.moveTo(0, item.size * 0.18);
      context.lineTo(-item.size * 0.12, item.size * 0.55);
      context.lineTo(item.size * 0.04, item.size * 0.49);
      context.lineTo(-item.size * 0.02, item.size * 0.78);
      context.lineTo(item.size * 0.22, item.size * 0.35);
      context.lineTo(item.size * 0.07, item.size * 0.39);
      context.closePath();
      context.fill();
      context.restore();
    };

    const drawHeart = (item: FlyingObject) => {
      context.save();
      context.translate(item.x, item.y);
      const pulse = 1 + Math.sin(model.worldTime * 6) * 0.08;
      context.scale(pulse, pulse);
      context.fillStyle = '#ff91ad';
      context.strokeStyle = '#594e83';
      context.lineWidth = 2;
      context.font = `bold ${item.size}px serif`;
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.shadowColor = '#fff';
      context.shadowBlur = 15;
      context.strokeText('♥', 0, 0);
      context.fillText('♥', 0, 0);
      context.restore();
    };

    const drawScene = () => {
      const { width, height } = model;
      const ratio = Math.min(window.devicePixelRatio || 1, 2);
      context.setTransform(ratio, 0, 0, ratio, 0, 0);
      context.clearRect(0, 0, width, height);
      const theme = BACKGROUND_THEMES[model.level];
      const sky = context.createLinearGradient(0, 0, 0, height);
      sky.addColorStop(0, theme.top);
      sky.addColorStop(0.52, theme.middle);
      sky.addColorStop(1, theme.bottom);
      context.fillStyle = sky;
      context.fillRect(0, 0, width, height);

      if (model.level === 4) {
        context.save();
        context.globalAlpha = 0.34;
        context.lineCap = 'round';
        ['#72ffd2', '#9cb9ff', '#e4aaff'].forEach((color, index) => {
          context.strokeStyle = color;
          context.lineWidth = 22 - index * 4;
          context.beginPath();
          context.moveTo(-40, height * (0.22 + index * 0.08));
          context.bezierCurveTo(width * 0.28, height * (0.02 + index * 0.08), width * 0.62, height * (0.48 - index * 0.03), width + 50, height * (0.13 + index * 0.06));
          context.stroke();
        });
        context.restore();
      }

      context.save();
      model.stars.forEach((star) => {
        const twinkle = 0.5 + Math.sin(model.worldTime * 2.2 + star.x) * 0.5;
        context.globalAlpha = star.alpha * (0.58 + twinkle * 0.42);
        context.fillStyle = model.level >= 6 ? '#ffd9c5' : '#fffbe2';
        context.beginPath();
        context.arc(star.x % (width + 20), star.y % (height * 0.78), star.size, 0, Math.PI * 2);
        context.fill();
      });
      context.restore();

      const moonX = width * 0.79;
      const moonY = height * 0.2;
      context.save();
      context.shadowColor = theme.celestial;
      context.shadowBlur = 42;
      context.fillStyle = theme.celestial;
      context.beginPath();
      context.arc(moonX, moonY, Math.min(model.level === 7 ? 76 : 60, width * 0.08), 0, Math.PI * 2);
      context.fill();
      context.shadowBlur = 0;
      if (model.level === 0 || model.level === 2 || model.level === 3) {
        context.fillStyle = theme.middle;
        context.beginPath();
        context.arc(moonX - 22, moonY - 15, Math.min(61, width * 0.071), 0, Math.PI * 2);
        context.fill();
      } else if (model.level === 6) {
        context.fillStyle = '#161126';
        context.beginPath();
        context.arc(moonX - 9, moonY + 2, Math.min(54, width * 0.065), 0, Math.PI * 2);
        context.fill();
      }
      context.restore();

      const horizon = height * 0.87;
      context.fillStyle = theme.silhouette;
      context.strokeStyle = theme.silhouette;
      context.lineWidth = 8;

      if (model.level === 0) {
        context.beginPath();
        context.moveTo(0, horizon);
        for (let x = 0; x <= width + 50; x += 45) {
          const roof = horizon - 22 - ((x / 45) % 3) * 13;
          context.lineTo(x, roof);
          context.lineTo(x + 18, roof - 18);
          context.lineTo(x + 36, roof);
          context.lineTo(x + 45, roof);
        }
        context.lineTo(width, height);
        context.lineTo(0, height);
        context.closePath();
        context.fill();
      } else if (model.level === 1) {
        context.fillRect(0, horizon - 18, width, height - horizon + 18);
        for (let x = 0; x < width; x += 80) context.fillRect(x, horizon - 44 - (x % 160 ? 20 : 0), 58, 70);
        const towerX = width * 0.72;
        context.fillRect(towerX - 35, horizon - 215, 70, 230);
        context.beginPath();
        context.moveTo(towerX - 48, horizon - 215);
        context.lineTo(towerX, horizon - 282);
        context.lineTo(towerX + 48, horizon - 215);
        context.fill();
        context.fillStyle = theme.celestial;
        context.beginPath();
        context.arc(towerX, horizon - 180, 21, 0, Math.PI * 2);
        context.fill();
        context.strokeStyle = theme.silhouette;
        context.lineWidth = 3;
        context.beginPath();
        context.moveTo(towerX, horizon - 180);
        context.lineTo(towerX, horizon - 194);
        context.moveTo(towerX, horizon - 180);
        context.lineTo(towerX + 11, horizon - 174);
        context.stroke();
      } else if (model.level === 2) {
        context.globalAlpha = 0.42;
        context.fillStyle = '#f8a8b8';
        for (let y = horizon - 82; y < height; y += 18) context.fillRect(0, y, width, 2);
        context.globalAlpha = 1;
        context.fillStyle = theme.silhouette;
        context.fillRect(0, horizon, width, height - horizon);
        context.strokeStyle = '#312f58';
        context.lineWidth = 3;
        context.beginPath();
        context.moveTo(0, horizon - 120);
        context.quadraticCurveTo(width * 0.5, horizon - 72, width, horizon - 130);
        context.stroke();
        for (let x = 70; x < width; x += 120) {
          context.fillStyle = '#ff9db2';
          roundedRect(context, x, horizon - 112 + Math.sin(x) * 8, 25, 34, 8);
          context.fill();
        }
      } else if (model.level === 3) {
        context.fillRect(0, horizon - 9, width, height - horizon + 9);
        context.fillRect(width * 0.72, horizon - 162, 34, 165);
        context.beginPath();
        context.moveTo(width * 0.70, horizon - 162);
        context.lineTo(width * 0.74, horizon - 205);
        context.lineTo(width * 0.78, horizon - 162);
        context.fill();
        context.fillStyle = theme.light;
        context.beginPath();
        context.arc(width * 0.74, horizon - 150, 10, 0, Math.PI * 2);
        context.fill();
        context.globalAlpha = 0.35;
        for (let x = 40; x < width; x += 92) context.fillRect(x, horizon + 12, 4, 60);
        context.globalAlpha = 1;
      } else if (model.level === 4) {
        context.beginPath();
        context.moveTo(0, height);
        context.lineTo(0, horizon - 45);
        for (let x = 0; x <= width; x += 110) {
          context.lineTo(x + 45, horizon - 110 - (x % 220 ? 35 : 0));
          context.lineTo(x + 110, horizon - 35);
        }
        context.lineTo(width, height);
        context.closePath();
        context.fill();
        context.strokeStyle = '#dff7f4';
        context.lineWidth = 7;
        for (let x = 0; x < width; x += 110) {
          context.beginPath();
          context.moveTo(x + 13, horizon - 66);
          context.lineTo(x + 45, horizon - 110 - (x % 220 ? 35 : 0));
          context.lineTo(x + 70, horizon - 77);
          context.stroke();
        }
      } else if (model.level === 5) {
        context.fillRect(0, horizon, width, height - horizon);
        const gateX = width * 0.69;
        context.fillRect(gateX - 82, horizon - 177, 15, 182);
        context.fillRect(gateX + 67, horizon - 177, 15, 182);
        context.fillRect(gateX - 104, horizon - 190, 208, 18);
        context.fillRect(gateX - 91, horizon - 215, 182, 15);
        context.fillStyle = theme.light;
        for (let x = 46; x < width; x += 130) {
          context.beginPath();
          context.arc(x, horizon - 55 - (x % 260 ? 24 : 0), 9, 0, Math.PI * 2);
          context.fill();
        }
      } else if (model.level === 6) {
        context.fillRect(0, horizon, width, height - horizon);
        for (let x = 20; x < width; x += 105) {
          const towerHeight = 75 + (x % 210 ? 65 : 0);
          context.fillRect(x, horizon - towerHeight, 62, towerHeight);
          context.beginPath();
          context.moveTo(x - 9, horizon - towerHeight);
          context.lineTo(x + 31, horizon - towerHeight - 54);
          context.lineTo(x + 71, horizon - towerHeight);
          context.fill();
        }
      } else {
        context.beginPath();
        context.moveTo(0, height);
        context.lineTo(0, horizon - 30);
        for (let x = 0; x <= width; x += 70) {
          context.lineTo(x + 22, horizon - 90 - (x % 140 ? 46 : 0));
          context.lineTo(x + 70, horizon - 18);
        }
        context.lineTo(width, height);
        context.closePath();
        context.fill();
        context.fillStyle = theme.light;
        context.globalAlpha = 0.82;
        for (let x = 16; x < width; x += 62) context.fillRect(x, horizon + 14 + (x % 3) * 8, 7, 30 + (x % 4) * 9);
        context.globalAlpha = 1;
      }

      context.globalAlpha = 0.82;
      context.fillStyle = theme.light;
      for (let x = 18; x < width; x += 68) context.fillRect(x, horizon + 3 + ((x / 68) % 2) * 10, 5, 7);
      context.globalAlpha = 1;

      if (model.dashTime > 0) {
        const trail = context.createLinearGradient(model.player.x - 180, 0, model.player.x + 70, 0);
        trail.addColorStop(0, '#ffe78500');
        trail.addColorStop(0.55, '#ffe78599');
        trail.addColorStop(1, '#fffbe5');
        context.strokeStyle = trail;
        context.lineWidth = 9;
        context.lineCap = 'round';
        context.beginPath();
        context.moveTo(model.player.x - 190, model.player.y + 70);
        context.lineTo(model.player.x + 45, model.player.y + 70);
        context.stroke();
      }

      model.objects.forEach((item) => {
        if (item.kind === 'letter') drawEnvelope(item);
        else if (item.kind === 'cloud') drawCloud(item);
        else drawHeart(item);
      });

      model.particles.forEach((particle) => {
        context.globalAlpha = Math.max(0, particle.life / particle.maxLife);
        context.fillStyle = particle.color;
        context.beginPath();
        context.arc(particle.x, particle.y, particle.size, 0, Math.PI * 2);
        context.fill();
      });
      context.globalAlpha = 1;

      const image = characterImagesRef.current[model.character];
      const flicker = model.invincible > 0 && model.dashTime <= 0 && Math.floor(model.invincible * 12) % 2 === 0;
      if (image?.complete && image.naturalWidth && !flicker) {
        const maxDrawWidth = model.character === 'frieren' ? 194 : 226;
        const maxDrawHeight = model.character === 'frieren' ? 205 : 190;
        const scale = Math.min(maxDrawWidth / image.naturalWidth, maxDrawHeight / image.naturalHeight, width * 0.29 / image.naturalWidth);
        const drawWidth = image.naturalWidth * scale;
        const drawHeight = image.naturalHeight * scale;
        const drawX = model.player.x + 52 - drawWidth * (model.character === 'frieren' ? 0.45 : 0.32);
        const drawY = model.player.y + 42 - drawHeight * (model.character === 'frieren' ? 0.5 : 0.34);
        context.save();
        context.shadowColor = model.dashTime > 0 ? '#fff4a8' : '#39335a55';
        context.shadowBlur = model.dashTime > 0 ? 26 : 12;
        context.drawImage(image, drawX, drawY, drawWidth, drawHeight);
        context.restore();
      }
    };

    const intersects = (item: FlyingObject) => {
      const playerWidth = Math.min(112, model.width * 0.15);
      const playerHeight = playerWidth * 0.52;
      const px = model.player.x + playerWidth * 0.05;
      const py = model.player.y + playerHeight * 0.05;
      const itemRadius = item.kind === 'cloud' ? item.size * 0.46 : item.size * 0.42;
      const closestX = Math.max(px, Math.min(item.x, px + playerWidth));
      const closestY = Math.max(py, Math.min(item.y, py + playerHeight));
      const dx = item.x - closestX;
      const dy = item.y - closestY;
      return dx * dx + dy * dy < itemRadius * itemRadius;
    };

    const spawnObject = () => {
      const config = LEVELS[model.level];
      const tierStart = ENDLESS_THRESHOLDS[model.level];
      const tierEnd = model.level < ENDLESS_THRESHOLDS.length - 1 ? ENDLESS_THRESHOLDS[model.level + 1] : tierStart + 60;
      const progress = model.mode === 'endless'
        ? Math.min(1, Math.max(0, (model.totalDelivered - tierStart) / (tierEnd - tierStart)))
        : 1 - model.time / config.time;
      const roll = Math.random();
      let kind: ObjectKind = roll < 1 - config.cloudRate ? 'letter' : 'cloud';
      if (roll > 0.985 && model.lives < CHARACTERS[model.character].maxLives) kind = 'heart';
      const size = kind === 'cloud' ? 72 + Math.random() * 25 : kind === 'heart' ? 35 : 39 + Math.random() * 9;
      model.objects.push({
        id: model.nextId++,
        kind,
        x: model.width + size,
        y: 55 + Math.random() * Math.max(180, model.height - 150),
        vx: -(165 + progress * 90 + Math.random() * 45) * config.speed * (kind === 'cloud' ? config.cloudSpeed : 1),
        size,
        spin: (Math.random() - 0.5) * 0.2,
      });
      model.spawnIn = Math.max(0.27, config.spawnBase - progress * 0.16) + Math.random() * 0.28;
    };

    const finish = (victory: boolean) => {
      if (statusRef.current !== 'playing') return;
      setGameStatus(victory ? 'victory' : 'gameover');
      const finalScore = Math.round(model.score);
      setHighScore((previous) => {
        const next = Math.max(previous, finalScore);
        window.localStorage.setItem('starry-post-high-score', String(next));
        return next;
      });
      setUi({
        score: finalScore,
        combo: model.combo,
        energy: Math.round(model.energy),
        lives: model.lives,
        mode: model.mode,
        level: model.level,
        delivered: model.mode === 'endless' ? model.totalDelivered : model.delivered,
        target: model.mode === 'endless' && model.level < ENDLESS_THRESHOLDS.length - 1
          ? ENDLESS_THRESHOLDS[model.level + 1]
          : LEVELS[model.level].target,
        time: Math.max(0, Math.ceil(model.time)),
      });
      stopBgm();
      playSound('end');
    };

    const update = (delta: number, now: number) => {
      model.worldTime += delta;
      model.stars.forEach((star) => {
        star.x -= star.speed * delta;
        if (star.x < -4) star.x = model.width + 4;
      });
      if (statusRef.current !== 'playing') return;

      if (model.mode === 'campaign') model.time = Math.max(0, model.time - delta);
      model.spawnIn -= delta;
      model.dashTime = Math.max(0, model.dashTime - delta);
      model.invincible = Math.max(0, model.invincible - delta);
      if (model.spawnIn <= 0) spawnObject();

      const keys = keysRef.current;
      const horizontal = (keys.has('arrowright') || keys.has('d') ? 1 : 0) - (keys.has('arrowleft') || keys.has('a') ? 1 : 0);
      const vertical = (keys.has('arrowdown') || keys.has('s') ? 1 : 0) - (keys.has('arrowup') || keys.has('w') ? 1 : 0);
      const characterConfig = CHARACTERS[model.character];
      const speed = (model.dashTime > 0 ? 430 : 290) * characterConfig.moveSpeed;
      const smoothing = Math.min(1, delta * 11);
      if (pointerRef.current.active) {
        const targetX = pointerRef.current.x - 52;
        const targetY = pointerRef.current.y - 42;
        model.player.x += (targetX - model.player.x) * Math.min(1, delta * 9 * characterConfig.moveSpeed);
        model.player.y += (targetY - model.player.y) * Math.min(1, delta * 9 * characterConfig.moveSpeed);
        model.player.vx *= 0.8;
        model.player.vy *= 0.8;
      } else {
        model.player.vx += (horizontal * speed - model.player.vx) * smoothing;
        model.player.vy += (vertical * speed - model.player.vy) * smoothing;
        model.player.x += model.player.vx * delta;
        model.player.y += model.player.vy * delta;
      }
      const maxX = model.width - Math.min(180, model.width * 0.25);
      const maxY = model.height - Math.min(125, model.height * 0.21);
      model.player.x = Math.max(4, Math.min(maxX, model.player.x));
      model.player.y = Math.max(8, Math.min(maxY, model.player.y));

      const remaining: FlyingObject[] = [];
      model.objects.forEach((item) => {
        if (model.dashTime > 0 && item.kind === 'letter') {
          const dx = model.player.x + 75 - item.x;
          const dy = model.player.y + 45 - item.y;
          const distance = Math.max(1, Math.hypot(dx, dy));
          if (distance < characterConfig.magnetRadius) {
            item.x += (dx / distance) * 450 * delta;
            item.y += (dy / distance) * 450 * delta;
          }
        }
        item.x += item.vx * delta * (model.dashTime > 0 ? 1.18 : 1);
        item.spin += delta * (item.kind === 'letter' ? 0.8 : 0.15);
        if (item.x < -item.size * 2) {
          if (item.kind === 'letter') model.combo = 0;
          return;
        }
        if (!intersects(item)) {
          remaining.push(item);
          return;
        }
        if (item.kind === 'letter') {
          model.combo += 1;
          const letterValue = characterConfig.letterValue;
          model.delivered += letterValue;
          model.totalDelivered += letterValue;
          if (model.mode === 'endless') {
            model.delivered = model.totalDelivered;
            const nextTier = getEndlessLevel(model.totalDelivered);
            if (nextTier > model.level) {
              const previousConfig = LEVELS[model.level];
              const nextConfig = LEVELS[nextTier];
              const cloudRatio = (nextConfig.speed * nextConfig.cloudSpeed) / (previousConfig.speed * previousConfig.cloudSpeed);
              const letterRatio = nextConfig.speed / previousConfig.speed;
              model.objects.forEach((object) => {
                object.vx *= object.kind === 'cloud' ? cloudRatio : letterRatio;
              });
              model.level = nextTier;
              burst(item.x, item.y, '#ffdf72', 28);
              playSound('dash');
            }
          }
          const multiplier = Math.min(5, 1 + Math.floor(model.combo / 5));
          model.score += 100 * multiplier * letterValue;
          model.energy = Math.min(100, model.energy + 12 + multiplier * 2);
          burst(item.x, item.y, '#ffe06d', 13);
          playSound('catch');
        } else if (item.kind === 'heart') {
          model.lives = Math.min(characterConfig.maxLives, model.lives + 1);
          model.score += 250;
          burst(item.x, item.y, '#ff9fba', 16);
          playSound('catch');
        } else if (model.dashTime > 0) {
          model.score += 50;
          burst(item.x, item.y, '#d9d2ff', 18);
        } else if (model.invincible <= 0) {
          model.lives -= 1;
          model.combo = 0;
          model.energy = Math.max(0, model.energy - 30);
          model.invincible = 1.55;
          burst(item.x, item.y, '#a8a2ca', 18);
          playSound('hit');
        } else {
          remaining.push(item);
        }
      });
      model.objects = remaining;

      model.particles.forEach((particle) => {
        particle.life -= delta;
        particle.x += particle.vx * delta;
        particle.y += particle.vy * delta;
        particle.vy += 38 * delta;
      });
      model.particles = model.particles.filter((particle) => particle.life > 0);

      if (now - model.lastUiUpdate > 80) {
        model.lastUiUpdate = now;
        setUi({
          score: Math.round(model.score),
          combo: model.combo,
          energy: Math.round(model.energy),
          lives: model.lives,
          mode: model.mode,
          level: model.level,
          delivered: model.mode === 'endless' ? model.totalDelivered : model.delivered,
          target: model.mode === 'endless' && model.level < ENDLESS_THRESHOLDS.length - 1
            ? ENDLESS_THRESHOLDS[model.level + 1]
            : LEVELS[model.level].target,
          time: Math.ceil(model.time),
        });
      }
      if (model.mode === 'campaign' && model.delivered >= LEVELS[model.level].target) {
        if (model.level === LEVELS.length - 1) finish(true);
        else {
          stopBgm();
          setGameStatus('levelclear');
          setUi({
            score: Math.round(model.score),
            combo: model.combo,
            energy: Math.round(model.energy),
            lives: model.lives,
            mode: model.mode,
            level: model.level,
            delivered: model.delivered,
            target: LEVELS[model.level].target,
            time: Math.ceil(model.time),
          });
          playSound('end');
        }
      } else if (model.lives <= 0 || (model.mode === 'campaign' && model.time <= 0)) finish(false);
    };

    let frame = 0;
    let lastTime = performance.now();
    const loop = (now: number) => {
      const delta = Math.min(0.034, Math.max(0, (now - lastTime) / 1000));
      lastTime = now;
      update(delta, now);
      drawScene();
      frame = requestAnimationFrame(loop);
    };
    frame = requestAnimationFrame(loop);
    return () => {
      cancelAnimationFrame(frame);
      observer.disconnect();
    };
  }, [playSound, setGameStatus, stopBgm]);

  const updatePointer = (event: React.PointerEvent<HTMLCanvasElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    pointerRef.current.x = event.clientX - bounds.left;
    pointerRef.current.y = event.clientY - bounds.top;
  };

  const scoreLabel = ui.score.toString().padStart(5, '0');
  const multiplier = Math.min(5, 1 + Math.floor(ui.combo / 5));
  const levelConfig = LEVELS[ui.level];
  const endlessTierStart = ENDLESS_THRESHOLDS[ui.level];
  const endlessTierEnd = ui.level < ENDLESS_THRESHOLDS.length - 1 ? ENDLESS_THRESHOLDS[ui.level + 1] : null;
  const deliveryProgress = ui.mode === 'endless'
    ? endlessTierEnd === null
      ? 100
      : Math.min(100, ((ui.delivered - endlessTierStart) / (endlessTierEnd - endlessTierStart)) * 100)
    : Math.min(100, (ui.delivered / ui.target) * 100);
  const deliveryLabel = ui.mode === 'endless'
    ? endlessTierEnd === null ? `${ui.delivered} 封` : `${ui.delivered}/${endlessTierEnd} 封`
    : `${ui.delivered}/${ui.target} 封`;
  const activeCharacter = CHARACTERS[selectedCharacter];

  return (
    <main className="game-shell">
      <div className="ambient ambient-one" aria-hidden="true" />
      <div className="ambient ambient-two" aria-hidden="true" />

      <header className="topbar">
        <div className="brand-mark" aria-hidden="true">星</div>
        <div className="brand-copy">
          <p>STARRY POST OFFICE · 第七夜航线</p>
          <h1>星穹邮差</h1>
        </div>
        <div className="top-actions">
          <div className="record-chip"><span>最高记录</span><strong>{highScore.toString().padStart(5, '0')}</strong></div>
          {isShareVersion && (
            <label className="music-picker" title={customMusicName || '从设备选择你拥有的 MP3，音乐仅在本机播放'}>
              <span>{customMusicName ? 'STYX HELIX 已载入' : '选择 STYX HELIX'}</span>
              <input type="file" accept="audio/mpeg,audio/mp3,.mp3" onChange={chooseCustomBgm} />
            </label>
          )}
          <button
            type="button"
            className="round-button"
            aria-label={muted ? '开启音乐与音效' : '关闭音乐与音效'}
            onClick={() => {
              const next = !muted;
              mutedRef.current = next;
              setMuted(next);
              if (next) {
                stopBgm();
              }
              else if (statusRef.current === 'playing') startBgm();
            }}
          >{muted ? '×' : '♪'}</button>
        </div>
      </header>

      <section className={`game-card ${ui.energy >= 100 && (status === 'playing' || status === 'paused') ? 'dash-ready' : ''}`}>
        <div className="hud">
          <div className="level-progress">
            <div><span>{ui.mode === 'endless' ? `速度 ${ui.level + 1} / 8 档` : `第 ${ui.level + 1} / ${LEVELS.length} 关`}</span><b>{ui.mode === 'endless' && ui.level === 7 ? 'MAX SPEED' : levelConfig.name}</b></div>
            <i><em style={{ width: `${deliveryProgress}%` }} /></i>
            <small>{deliveryLabel}</small>
          </div>
          <div className="hud-block score-block"><span>本次得分</span><b>{scoreLabel}</b></div>
          <div className="hud-block"><span>星笺连击</span><b>{ui.combo} <em>×{multiplier}</em></b></div>
          <div className="lives" aria-label={`剩余 ${ui.lives} 点生命`}>
            {Array.from({ length: activeCharacter.maxLives }, (_, heart) => <i key={heart} className={heart < ui.lives ? 'active' : ''}>♥</i>)}
          </div>
          <div className="time-chip"><span>{ui.mode === 'endless' ? '模式' : '剩余时间'}</span><strong>{Number.isFinite(ui.time) ? ui.time : '∞'}{Number.isFinite(ui.time) && <small>s</small>}</strong></div>
          <button
            type="button"
            className="energy-meter"
            aria-label={ui.energy >= 100 ? '释放流星冲刺' : `流星能量 ${ui.energy}%`}
            disabled={ui.energy < 100 || (status !== 'playing' && status !== 'paused')}
            onClick={resumeAndDash}
          >
            <div><span>流星能量</span><b>{ui.energy >= 100 ? 'READY!' : `${ui.energy}%`}</b></div>
            <i><em style={{ width: `${ui.energy}%` }} /></i>
          </button>
        </div>

        <div ref={stageRef} className="playfield">
          <canvas
            ref={canvasRef}
            aria-label="星穹邮差游戏区域。使用方向键或 WASD 移动，空格释放流星冲刺。"
            onPointerDown={(event) => {
              if (event.pointerType === 'mouse') return;
              event.currentTarget.setPointerCapture(event.pointerId);
              pointerRef.current.active = true;
              updatePointer(event);
            }}
            onPointerMove={(event) => {
              if (event.pointerType === 'mouse') return;
              if (pointerRef.current.active) updatePointer(event);
            }}
            onPointerUp={() => { pointerRef.current.active = false; }}
            onPointerCancel={() => { pointerRef.current.active = false; }}
          />

          {status === 'menu' && (
            <div className="overlay intro-panel">
              <div className="chapter-line"><span>SELECT NIGHT ROUTE</span><i /></div>
              <p className="kicker">今晚的信件，会变成谁的愿望？</p>
              <h2>把今晚的星光<br />送到每一扇窗前</h2>
              <p className="intro-copy">选择你的夜航方式。收集星笺、避开暴雨云，蓄满能量即可冲破夜空。</p>
              <div className="character-selector" aria-label="选择角色">
                {CHARACTER_IDS.map((characterId) => {
                  const character = CHARACTERS[characterId];
                  return (
                    <button
                      key={characterId}
                      type="button"
                      className={`character-card ${selectedCharacter === characterId ? 'selected' : ''}`}
                      aria-pressed={selectedCharacter === characterId}
                      onClick={() => chooseCharacter(characterId)}
                    >
                      <span className="character-portrait"><img src={character.sprite} alt="" /></span>
                      <span className="character-copy"><strong>{character.name}</strong><small>{character.title}</small><b>{character.perk}</b></span>
                    </button>
                  );
                })}
              </div>
              <div className="mode-selector">
                <button type="button" className="mode-card campaign-mode" onClick={() => startGame('campaign')}>
                  <span>STORY ROUTE</span>
                  <strong>闯关模式</strong>
                  <small>八关路线 · 限时投递</small>
                  <b>开始挑战 →</b>
                </button>
                <button type="button" className="mode-card endless-mode" onClick={() => startGame('endless')}>
                  <span>ENDLESS FLIGHT</span>
                  <strong>无尽模式</strong>
                  <small>八档速度 · 无限积分</small>
                  <b>开始挑战 ∞</b>
                </button>
              </div>
              <div className="control-grid">
                <span><kbd>WASD</kbd><b>移动</b></span>
                <span><kbd>SPACE</kbd><b>流星冲刺</b></span>
                <span><kbd>拖动</kbd><b>触屏移动</b></span>
              </div>
            </div>
          )}

          {status === 'paused' && (
            <div className="overlay pause-panel">
              <span className="mini-star">✦</span>
              <p>夜风稍歇</p>
              <h2>航线已暂停</h2>
              <button type="button" className="primary-button" onClick={togglePause}>继续夜航 <span>→</span></button>
              <small>{ui.energy >= 100 ? '按空格继续并立即冲刺' : '按 P 或空格继续'}</small>
            </div>
          )}

          {status === 'levelclear' && (
            <div className="overlay level-clear-panel">
              <span className="clear-stamp">CLEAR</span>
              <p>第 {ui.level + 1} 关 · {levelConfig.name}</p>
              <h2>投递完成！</h2>
              <div className="next-route">
                <span>下一站</span>
                <strong>{LEVELS[ui.level + 1].name}</strong>
                <small>{LEVELS[ui.level + 1].subtitle}</small>
              </div>
              <button type="button" className="primary-button" onClick={startNextLevel}>前往下一关 <span>→</span></button>
              <small className="space-tip">按空格也可以继续</small>
            </div>
          )}

          {status === 'gameover' && (
            <div className="overlay result-panel">
              <p className="result-eyebrow">DELIVERY INTERRUPTED</p>
              <h2>{ui.lives > 0 ? '时间已经不够了' : '下次会飞得更远'}</h2>
              <p className="failed-route">{ui.mode === 'endless' ? `无尽模式 · 共投递 ${ui.delivered} 封 · 速度 ${ui.level + 1} 档` : `止步于第 ${ui.level + 1} 关 · ${levelConfig.name}`}</p>
              <div className="final-score"><span>本次投递星光</span><strong>{scoreLabel}</strong></div>
              <div className="result-note">{ui.score >= highScore && ui.score > 0 ? '✦ 新的最高记录！' : `最高记录 · ${highScore.toString().padStart(5, '0')}`}</div>
              <button type="button" className="primary-button" onClick={() => startGame(ui.mode)}>再飞一程 <span>↻</span></button>
              <button type="button" className="text-button" onClick={() => setGameStatus('menu')}>返回任务简报</button>
            </div>
          )}

          {status === 'victory' && (
            <div className="overlay result-panel victory-panel">
              <p className="result-eyebrow">ALL ROUTES COMPLETE</p>
              <h2>晨光抵达了！</h2>
              <p className="victory-copy">八条夜航路线全部完成，今晚的愿望都已平安送达。</p>
              <div className="final-score"><span>最终投递星光</span><strong>{scoreLabel}</strong></div>
              <div className="result-note">✦ 第 7 邮局授予你「星穹邮差」徽章</div>
              <button type="button" className="primary-button" onClick={() => startGame('campaign')}>再次挑战 <span>↻</span></button>
              <button type="button" className="text-button" onClick={() => setGameStatus('menu')}>返回任务简报</button>
            </div>
          )}

          {status === 'playing' && (
            <>
              <button type="button" className="pause-button" aria-label="暂停游戏" onClick={togglePause}>Ⅱ</button>
              {ui.energy >= 100 && <button type="button" className="dash-button" onClick={resumeAndDash}>流星冲刺 <span>SPACE</span></button>}
            </>
          )}
        </div>
      </section>

      <footer className="game-footer">
        <span><i>✦</i> 今夜天气：晴，偶有流星</span>
        <span>方向键 / WASD 移动 · P 暂停</span>
        <span>♪ 夜航 BGM · 完整循环</span>
      </footer>
    </main>
  );
}
