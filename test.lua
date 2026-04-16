local kps = require("kpsintent")

-- Usage: kps.play(frequency_or_table, duration, [decay], [brightness], [volume])
-- decay: 0.0 to 1.0 (sustain)
-- brightness: 0.0 to 1.0 (filter coefficient - lower is brighter)
-- volume: 0.0 to 1.0

print("Playing an A Minor chord (A3, C4, E4)...")
-- Note: When playing multiple notes, lower the volume to avoid clipping
kps.play({220.0, 261.63, 329.63}, 2.5, 0.997, 0.2, 0.3)

print("Done.")
