from __future__ import annotations

import gc
import os
import sys
from pathlib import Path

import imageio.v2 as imageio
import numpy as np
import torch
import yaml
from accelerate import cpu_offload
from accelerate.hooks import remove_hook_from_module
from transformers import T5EncoderModel, T5Tokenizer


ROOT = Path(__file__).resolve().parent
LTX_ROOT = ROOT / "LTX-Video"
sys.path.insert(0, str(LTX_ROOT))

from generate_ltx_clips import CLIPS  # noqa: E402
from ltx_video.inference import (  # noqa: E402
    create_ltx_video_pipeline,
    prepare_conditioning,
)
from ltx_video.utils.skip_layer_strategy import SkipLayerStrategy  # noqa: E402


CONFIG_PATH = LTX_ROOT / "configs" / "ltxv-2b-0.9.8-distilled-lowvram.yaml"
OUT = ROOT / "ltx_clips"
TOKENIZER_DIR = Path("C:/Users/HP/AppData/Local/Temp/ltx_tokenizer")
EMBED_CACHE = ROOT / "ltx_prompt_embeddings.pt"
NEGATIVE_PROMPT = (
    "still image, slideshow, frozen character, no body motion, locked face, bad lip sync, "
    "worst quality, inconsistent motion, jitter, flicker, distorted face, extra fingers, broken hands, "
    "identity change, wrong clothes, live action, photorealistic, subtitles, text, logo"
)


@torch.inference_mode()
def encode_prompts(text_encoder_path: str) -> tuple[list[dict[str, torch.Tensor]], dict[str, torch.Tensor]]:
    print("Loading T5 text encoder for one-time prompt encoding...", flush=True)
    tokenizer = T5Tokenizer.from_pretrained(TOKENIZER_DIR)
    encoder = T5EncoderModel.from_pretrained(
        text_encoder_path,
        subfolder="text_encoder",
        torch_dtype=torch.bfloat16,
        low_cpu_mem_usage=True,
    ).eval()
    cpu_offload(encoder, torch.device("cuda"), offload_buffers=True)

    def encode(text: str) -> dict[str, torch.Tensor]:
        tokens = tokenizer(
            [text],
            padding="max_length",
            max_length=256,
            truncation=True,
            add_special_tokens=True,
            return_tensors="pt",
        )
        input_ids = tokens.input_ids.to("cuda")
        attention_mask = tokens.attention_mask.to("cuda")
        embeddings = encoder(input_ids, attention_mask=attention_mask)[0]
        return {
            "embeddings": embeddings.cpu(),
            "mask": attention_mask.cpu(),
        }

    negative = encode(NEGATIVE_PROMPT)
    encoded = []
    for clip in CLIPS:
        print(f"Encoding prompt: {clip['name']}", flush=True)
        encoded.append(encode(clip["prompt"]))

    remove_hook_from_module(encoder, recurse=True)
    del encoder, tokenizer
    gc.collect()
    torch.cuda.empty_cache()
    return encoded, negative


def write_video(images: torch.Tensor, path: Path, fps: int = 24) -> None:
    video = images[0].permute(1, 2, 3, 0).cpu().float().numpy()
    video = np.clip(video * 255.0, 0, 255).astype(np.uint8)
    path.parent.mkdir(parents=True, exist_ok=True)
    with imageio.get_writer(path, fps=fps, codec="libx264", quality=8) as writer:
        for frame in video:
            writer.append_data(frame)


@torch.inference_mode()
def main() -> None:
    os.environ.setdefault("HF_HOME", str(ROOT / "hf_cache"))
    os.environ.setdefault("HF_HUB_OFFLINE", "1")
    with CONFIG_PATH.open("r", encoding="utf-8") as handle:
        config = yaml.safe_load(handle)

    if not EMBED_CACHE.exists():
        prompt_data, negative_data = encode_prompts(config["text_encoder_model_name_or_path"])
        torch.save(
            {"prompts": prompt_data, "negative": negative_data},
            EMBED_CACHE,
        )
        print(f"Saved prompt embeddings to {EMBED_CACHE}. Restart the script for video generation.", flush=True)
        return

    cached = torch.load(EMBED_CACHE, map_location="cpu", weights_only=True)
    prompt_data = cached["prompts"]
    negative_data = cached["negative"]

    print("Loading LTX 2B distilled video model...", flush=True)
    pipeline = create_ltx_video_pipeline(
        ckpt_path=config["checkpoint_path"],
        precision=config["precision"],
        text_encoder_model_name_or_path=None,
        sampler=config["sampler"],
        device="cuda",
        enhance_prompt=False,
        offload_to_cpu=True,
    )

    for clip, prompt in zip(CLIPS, prompt_data):
        num_frames = clip["num_frames"]
        output_path = OUT / clip["name"] / f"{clip['name']}.mp4"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        if output_path.exists():
            print(f"Skipping existing {output_path}", flush=True)
            continue
        print(f"Generating {clip['name']} ({num_frames} frames)...", flush=True)

        conditioning_items = prepare_conditioning(
            conditioning_media_paths=[str(path) for path in clip["images"]],
            conditioning_strengths=[1.0] * len(clip["images"]),
            conditioning_start_frames=clip["frames"],
            height=448,
            width=704,
            num_frames=num_frames,
            padding=(0, 0, 0, 0),
            pipeline=pipeline,
        )
        generator = torch.Generator(device="cuda").manual_seed(clip["seed"])
        result = pipeline(
            prompt=None,
            negative_prompt=None,
            prompt_embeds=prompt["embeddings"],
            prompt_attention_mask=prompt["mask"],
            negative_prompt_embeds=negative_data["embeddings"],
            negative_prompt_attention_mask=negative_data["mask"],
            height=448,
            width=704,
            num_frames=num_frames,
            frame_rate=24,
            num_inference_steps=8,
            guidance_scale=1,
            stg_scale=0,
            rescaling_scale=1,
            decode_timestep=0.05,
            decode_noise_scale=0.025,
            sampler="from_checkpoint",
            stochastic_sampling=False,
            skip_layer_strategy=SkipLayerStrategy.AttentionValues,
            generator=generator,
            output_type="pt",
            media_items=None,
            conditioning_items=conditioning_items,
            is_video=True,
            vae_per_channel_normalize=True,
            image_cond_noise_scale=0.08,
            mixed_precision=False,
            offload_to_cpu=True,
            device="cuda",
            enhance_prompt=False,
        ).images
        write_video(result, output_path)
        print(f"Saved {output_path}", flush=True)
        del result, conditioning_items
        gc.collect()
        torch.cuda.empty_cache()


if __name__ == "__main__":
    main()
