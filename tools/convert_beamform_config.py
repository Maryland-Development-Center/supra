import json

config = "build/example-config.json"
out_file = "config/config-interson-gen.json"
print(f"Using {config} as base file")
pre = json.load(open(config))

# constants/etc
to_update = {
    "depth": 52.5397,
    "elementLayout": {"x": 128, "y": 1},
    "numElements": 128,
    "numReceivedChannels": 127,
    "numSamples": 2048,
    "numTxScanlines": 128,
    "rxNumDepths": 2048,
    "samplingFrequency": 7500000,
    "scanlineLayout": {"x": 128, "y": 1},
}

out = pre.copy()
for k, v in to_update.items():
    out[k] = v

out["rxElementPosition"] = pre["rxElementPosition"][:out["numElements"]]
out["rxScanlines"] = pre["rxScanlines"][::2]

print(f"number of rx scanlines: {len(out['rxScanlines'])}")
json.dump(out, open(out_file, "w"))