import torch
import struct
import sys
import os

def nnue_to_raw(nnue_path, out_path):
    # Load the PyTorch model
    model = torch.load(nnue_path, map_location='cpu')

    # The official repo uses these keys in state_dict
    state_dict = model['state_dict'] if 'state_dict' in model else model

    # Extract tensors
    feature_weights = state_dict['input_layer.weight'].cpu().numpy().flatten()
    feature_bias    = state_dict['input_layer.bias'].cpu().numpy().flatten()
    hidden_weights  = state_dict['output_layer.weight'].cpu().numpy().flatten()
    output_bias     = state_dict['output_layer.bias'].cpu().numpy().flatten()

    # Convert all to int32 for safety
    all_weights = list(feature_weights.astype('int32')) + \
                  list(feature_bias.astype('int32')) + \
                  list(hidden_weights.astype('int32')) + \
                  list(output_bias.astype('int32'))

    # Write as little-endian int32
    with open(out_path, "wb") as f:
        for w in all_weights:
            f.write(struct.pack("<i", int(w)))

    print(f"[+] Exported {len(all_weights)} weights to {out_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python nnue_to_raw.py <model.pt> <out.raw>")
        sys.exit(1)
    nnue_to_raw(sys.argv[1], sys.argv[2])
