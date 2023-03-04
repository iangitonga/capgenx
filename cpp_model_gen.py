"""
Converts whisper models released by OpenAI to a format that can readily be used by Capgen.
It basically does the following:

1. Load both the English and multilingual models into a torch module.
2. Saves the encoder and decoder modules for english and multilingual models separately.
3. Compresses the saved encoders and decoders into one zip file. The uncompressed zip file for e.g tiny, ends
   up looking like this.

tiny/
    encoder.en.pt  # English encoder
    encoder.pt     # Multilingual encoder
    decoder.en.pt  # English decoder
    decoder.pt     # Multilingual decoder

This format allows capgen app to select english models for english->english transcription
and multilingual models for other->other transcription or other->english translation without
user input. In this design, the user does not need to know about model details.
"""


import argparse
import zipfile
from dataclasses import dataclass
from pathlib import Path

import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np


# MODEL CONFIG PARAMETERS
@dataclass
class ModelDimensions: 
    n_mels: int = None  # Number of audio mel-frequency bins expected.
    n_vocab: int =  None  # Size of whisper Vocabulary.
    n_audio_ctx: int = None  # Number of frames(context length) in the encoder output representation.
    n_audio_state: int = None  # Dimensionality of each frame of the encoder output representation.
    n_audio_head: int = None  # Number of heads in the audio encoder multi-head self-attention layers.
    n_audio_layer: int = None  # Number of blocks in the encoder.
    n_text_ctx: int = None  # Max number of tokens to be used as a context in the decoder.
    n_text_state: int = None  # Dimensionality of each token embedding.
    n_text_head: int = None # Number of heads in the text decoder multi-head attention layers.
    n_text_layer: int = None  # Number of blocks in the decoder.


class MultiHeadSelfAttention(nn.Module):
    def __init__(self, n_head, n_state):
        super().__init__()
        
        self.n_head = n_head
        self.n_state = n_state
        self.d_head = n_state // n_head
        self.query = nn.Linear(n_state, n_head * self.d_head)
        self.key = nn.Linear(n_state, n_head * self.d_head, bias=False)
        self.value = nn.Linear(n_state, n_head * self.d_head)
        self.out = nn.Linear(n_head * self.d_head, n_state)
        
    def forward(self, x, mask=None):
        q = self.query(x)
        k = self.key(x)
        v = self.value(x)
        qkv = self._qkv_attention(q, k, v, mask)
        out = self.out(qkv)
        return out
    
    def _qkv_attention(self, q, k, v, mask=None):
        n_batch, ctx = q.shape[0], q.shape[1]
        scale = self.d_head ** -0.25
        q = q.view(n_batch, ctx, self.n_head, self.d_head).permute(0, 2, 1, 3) * scale
        k = k.view(n_batch, ctx, self.n_head, self.d_head).permute(0, 2, 3, 1) * scale
        v = v.view(n_batch, ctx, self.n_head, self.d_head).permute(0, 2, 1, 3)
        qk = q @ k
        if mask is not None:
            qk += mask[:ctx, :ctx]
        qk = F.softmax(qk, dim=-1)
        qkv = qk @ v
        qkv = qkv.permute(0, 2, 1, 3).flatten(start_dim=2)
        return qkv
    

class MultiHeadCrossAttention(nn.Module):
    def __init__(self, n_head, n_state):
        super().__init__()
        
        self.n_head = n_head
        self.n_state = n_state
        self.d_head = n_state // n_head
        self.query = nn.Linear(n_state, n_head * self.d_head)
        self.key = nn.Linear(n_state, n_head * self.d_head, bias=False)
        self.value = nn.Linear(n_state, n_head * self.d_head)
        self.out = nn.Linear(n_head * self.d_head, n_state)

        # NOTE: We use zeros instead of None so that torch script knows that
        # cache is of type torch::Tensor. The actual tensor is not embedded into
        # the generated code.
        self.k_cache = torch.zeros(1)
        self.v_cache = torch.zeros(1)
        self.cache_idx = torch.tensor(-1)
        
    def forward(self, x, xa, cache_idx):
        q = self.query(x)
        # The inequality operator on tensors will work because both tensors are singletons.
        if cache_idx != self.cache_idx:
            self.k_cache = self.key(xa)
            self.v_cache = self.value(xa)
            self.cache_idx = cache_idx
        qkv = self._qkv_attention(q, self.k_cache, self.v_cache)
        out = self.out(qkv)
        return out
    
    def _qkv_attention(self, q, k, v):
        n_batch, q_ctx = q.shape[0], q.shape[1]
        kv_ctx = k.shape[1]
        scale = self.d_head ** -0.25
        q = q.view(n_batch, q_ctx, self.n_head, self.d_head).permute(0, 2, 1, 3) * scale
        k = k.view(n_batch, kv_ctx, self.n_head, self.d_head).permute(0, 2, 3, 1) * scale
        v = v.view(n_batch, kv_ctx, self.n_head, self.d_head).permute(0, 2, 1, 3)
        qk = q @ k
        qk = F.softmax(qk, dim=-1)
        qkv = qk @ v
        qkv = qkv.permute(0, 2, 1, 3).flatten(start_dim=2)
        return qkv


class ResidualAttentionBlockEncoder(nn.Module):
    def __init__(self, n_state, n_head, n_mlp):
        super().__init__()
        
        self.attn = MultiHeadSelfAttention(n_head, n_state)
        self.attn_ln = nn.LayerNorm(n_state)

        self.mlp = nn.Sequential(nn.Linear(n_state, n_mlp), nn.GELU(), nn.Linear(n_mlp, n_state))
        self.mlp_ln = nn.LayerNorm(n_state)

    def forward(self, x):
        x = x + self.attn(self.attn_ln(x))
        x = x + self.mlp(self.mlp_ln(x))
        return x
    

class ResidualAttentionBlockDecoder(nn.Module):
    def __init__(self, n_state, n_head, n_mlp):
        super().__init__()
        
        self.attn = MultiHeadSelfAttention(n_head, n_state)
        self.attn_ln = nn.LayerNorm(n_state)

        self.cross_attn = MultiHeadCrossAttention(n_head, n_state)
        self.cross_attn_ln = nn.LayerNorm(n_state)

        self.mlp = nn.Sequential(nn.Linear(n_state, n_mlp), nn.GELU(), nn.Linear(n_mlp, n_state))
        self.mlp_ln = nn.LayerNorm(n_state)

    def forward(self, x, xa, mask, cache_idx):
        x = x + self.attn(self.attn_ln(x), mask)
        x = x + self.cross_attn(self.cross_attn_ln(x), xa, cache_idx)
        x = x + self.mlp(self.mlp_ln(x))
        return x
    
    
class AudioEncoder(nn.Module):
    def __init__(self, n_mels, n_audio_layer, n_audio_ctx, n_audio_state, n_audio_head):
        super().__init__()
        
        self.conv1 = nn.Conv1d(n_mels, n_audio_state, kernel_size=3, stride=1, padding=1)
        self.conv2 = nn.Conv1d(n_audio_state, n_audio_state, kernel_size=3, stride=2, padding=1)
        self.gelu = nn.GELU()
        self.register_buffer("positional_embedding", self._get_pos_encoding(n_audio_ctx, n_audio_state), persistent=True)

        n_audio_mlp = n_audio_state * 4
        self.blocks = nn.ModuleList(
            [ResidualAttentionBlockEncoder(n_audio_state, n_audio_head, n_audio_mlp) for _ in range(n_audio_layer)]
        )
        self.ln_post = nn.LayerNorm(n_audio_state)

    def forward(self, x):
        x = self.gelu(self.conv1(x))
        x = self.gelu(self.conv2(x))
        x = x.permute(0, 2, 1)

        x += self.positional_embedding

        for block in self.blocks:
            x = block(x)

        x = self.ln_post(x)
        return x
    
    def _get_pos_encoding(self, n_audio_ctx, n_audio_state):
        dim_mask = torch.arange(n_audio_state//2).view(1, -1)
        factor = torch.log(torch.tensor(10_000)) / (n_audio_state // 2 - 1)
        dim_mask = torch.exp(-factor * dim_mask)
        pos_mask = torch.arange(n_audio_ctx).view(n_audio_ctx, 1)
        mask = pos_mask * dim_mask
        pos_encoding = torch.cat((torch.sin(mask), torch.cos(mask)), dim=1)
        return pos_encoding
    

class TextDecoder(nn.Module):
    def __init__(self, n_vocab, n_text_layer, n_text_ctx, n_text_state, n_text_head):
        super().__init__()
        
        self.token_embedding = nn.Embedding(n_vocab, n_text_state)
        self.positional_embedding = nn.Parameter(torch.empty(n_text_ctx, n_text_state))
        n_text_mlp = n_text_state * 4
        self.blocks = nn.ModuleList(
            [ResidualAttentionBlockDecoder(n_text_state, n_text_head, n_text_mlp)
            for _ in range(n_text_layer)]
        )
        self.ln = nn.LayerNorm(n_text_state)

        mask = torch.full((n_text_ctx, n_text_ctx), float("-Infinity")).triu_(diagonal=1)
        # Mask made persistent to ensure it is exported along the model.
        self.register_buffer("mask", mask, persistent=True)

    def forward(self, x, xa, cache_idx):
        x = self.token_embedding(x) + self.positional_embedding[: x.shape[-1]]
        x = x.to(xa.dtype)
        for block in self.blocks:
            x = block(x, xa, mask=self.mask, cache_idx=cache_idx)
        x = self.ln(x)
        logits = (x @ torch.transpose(self.token_embedding.weight.to(x.dtype), 0, 1)).float()
        return logits
    

class Whisper(nn.Module):
    def __init__(self, dims):
        self.dims = dims
        super().__init__()

        self.encoder = AudioEncoder(
            n_mels=dims.n_mels,
            n_audio_layer=dims.n_audio_layer,
            n_audio_ctx=dims.n_audio_ctx,
            n_audio_state=dims.n_audio_state,
            n_audio_head=dims.n_audio_head, 
        )
        self.decoder = TextDecoder(
            n_vocab=dims.n_vocab, 
            n_text_layer=dims.n_text_layer, 
            n_text_ctx=dims.n_text_ctx,
            n_text_state=dims.n_text_state, 
            n_text_head=dims.n_text_head,
        )
    
    @torch.no_grad()
    def embed_audio(self, mel):
        return self.encoder.forward(mel)
    
    @torch.no_grad()
    def logits(self, tokens, audio_features, cache=None):
        return self.decoder.forward(tokens, audio_features, cache)

    @property
    def device(self):
        return next(self.parameters()).device

    @property
    def is_multilingual(self):
        return self.dims.n_vocab == 51865

def export_model(model_path, is_en: bool):
    print(f"Exporting {model_path}")
    model_name = Path(model_path).stem
    with open(model_path, "rb") as fp:
        checkpoint = torch.load(fp, map_location="cpu")
    model = Whisper(ModelDimensions(**checkpoint["dims"]))
    # Expected out put: _IncompatibleKeys(missing_keys=['decoder.mask'], unexpected_keys=[])
    model.load_state_dict(checkpoint["model_state_dict"], strict=False)

    # ENCODER
    encoder = model.encoder
    encoder = encoder.eval()
    # Disable grad tracking
    for param in encoder.parameters():
        param.requires_grad = False

    dummy_input = torch.randn((1, 80, 3000))
    # For encoder, trace because we don't use cache or masking if statements.
    encoder_module = torch.jit.trace(encoder, example_inputs=dummy_input)
    encoder_module = torch.jit.freeze(encoder_module)
    encoder_save_path = "encoder.en.pt" if is_en else "encoder.pt"
    encoder_module.save(encoder_save_path)

    # DECODER
    decoder = model.decoder
    decoder = decoder.eval()
    # Disable grad tracking
    for param in decoder.parameters():
        param.requires_grad = False

    dummy_x = torch.randint(0, model.dims.n_vocab, (1, 10), requires_grad=False)
    dummy_xa = torch.randn((1, 1500, 384), requires_grad=False)
    dummy_cache_idx = torch.tensor(0, requires_grad=False)
    # Script due to if statements.
    decoder_module = torch.jit.script(decoder, example_inputs=(dummy_x, dummy_xa, dummy_cache_idx))
    decoder_module = torch.jit.freeze(decoder_module)
    decoder_save_path = "decoder.en.pt" if is_en else "decoder.pt"
    decoder_module.save(decoder_save_path)
    print(f"Completed Exporting {model_path}")
    return [encoder_save_path, decoder_save_path]


def export(en_model_path, ml_model_path):  # ml -> multilingual
    exported = export_model(en_model_path, is_en=True)
    exported.extend(export_model(ml_model_path, is_en=False))
    print()
    print("ZIP compression started")

    model_name = Path(ml_model_path).stem
    with zipfile.ZipFile(f"{model_name}.zip", 'w') as outzip:
        for exported_path in exported:
            print(f"Compressing {exported_path} ...")
            outzip.write(exported_path, compress_type=zipfile.ZIP_DEFLATED)
    print("ZIP Compression Completed")


parser = argparse.ArgumentParser()
parser.add_argument("en_modelpath", help="path to the English model to be converted.")
parser.add_argument("ml_modelpath", help="path to the Multilingual model to be converted.")
args = parser.parse_args()
export(args.en_modelpath, args.ml_modelpath)
