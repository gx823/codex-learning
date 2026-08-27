from huggingface_hub import hf_hub_download, snapshot_download


def main() -> None:
    model = hf_hub_download(
        repo_id="Lightricks/LTX-Video",
        filename="ltxv-2b-0.9.8-distilled.safetensors",
        repo_type="model",
    )
    print(f"LTX model: {model}", flush=True)

    text_encoder = snapshot_download(
        repo_id="PixArt-alpha/PixArt-XL-2-1024-MS",
        allow_patterns=["text_encoder/*", "tokenizer/*"],
        repo_type="model",
    )
    print(f"Text encoder: {text_encoder}", flush=True)


if __name__ == "__main__":
    main()
