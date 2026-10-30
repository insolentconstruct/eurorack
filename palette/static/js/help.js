// Copyright 2026 Insolent Construct.
// MIT licence: see encoder.js.
// Links follow the selected release, including its source revision during development.
const docs = 'https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/FEATURES.md';
const descriptions = {
  1: ['Snap each grain’s pitch to a chosen scale.', 'drizzlet-options'],
  2: ['Choose the stock reverb or the Clouds-style reverb character.', 'decisions'],
  3: ['Keep automatic delay, disable it, or widen the delay filter’s neutral zone.', 'exact-option-values'],
  4: ['Choose granular playback or Clouds’ spectral processing.', 'drizzlet-options'],
  5: ['Choose the stock limiter, faster release, or no limiter.', 'drizzlet-options'],
  6: ['Let feedback slowly change the frozen sound instead of repeating it exactly.', 'drizzlet-options'],
  7: ['Choose the Beads wavetable synth or Plaits when synth mode is active.', 'drizzlet-options'],
  8: ['Start grains on recorded hits, with the option to trigger a grain on each hit.', 'drizzlet-options'],
  9: ['Start the synth automatically or by a gesture with both inputs unplugged and FREEZE off.', 'buttons-and-startup'],
  10: ['Choose continuous random pitch or fifths and octaves with the PITCH attenurandomizer counter-clockwise.', 'drizzlet-options'],
  11: ['Spread grains across the stereo field.', 'stereo-width'],
  12: ['Set how much diffusion softens the grains before reverb.', 'decisions'],
  13: ['Play grains one at a time; this works best with the square and decay envelopes.', 'buttons-and-startup'],
  14: ['Start a fresh recording window so the next freeze keeps only the new recording.', 'buttons-and-startup'],
  15: ['Show a light-blue version cue at startup without delaying audio.', 'small-beta-2-profile-options'],
  16: ['Extend manual input gain below unity down to silence, keeping automatic gain unchanged.', 'small-beta-2-profile-options'],
  17: ['Show a brief light cue when entering or leaving delay mode.', 'small-beta-2-profile-options'],
  18: ['Choose full, two-thirds or one-third LED brightness, including options and warnings.', 'small-beta-2-profile-options'],
  19: ['Make assignment LEDs follow the strength of the assignable CV.', 'small-beta-2-profile-options'],
  20: ['Keep FREEZE fully lit while saving the frozen buffer.', 'small-beta-2-profile-options'],
  21: ['While latched with SEED patched, keep the stock divider or free-run with Ignore, Sync or Add triggers.', 'small-beta-2-profile-options'],
  22: ['Show when the PITCH knob reaches unison or an octave, ignoring CV and random pitch.', 'small-beta-2-profile-options'],
  23: ['Scan the granular buffer with the TIME attenurandomizer when TIME CV is unplugged.', 'small-beta-2-profile-options']
};

export function featureHelp(feature, release) {
  const [description, section] = descriptions[feature.id];
  return { description, url: docs.replace('{TAG}', encodeURIComponent(release.development ? release.source_commit : release.tag)) + '#' + section };
}

export function actionHelp(feature) {
  if (feature.id === 13) return 'Adjustable uses an action to toggle monophonic grains; it has no page knob. Fixed keeps it on or off without a toggle.';
  if (feature.id === 14) return 'Buffer trim is an action, not a lasting setting: Fixed On or Adjustable enables the chosen gesture.';
  if (feature.id === 9) return 'Manual synth start needs an action; Fixed Manual keeps that gesture available without a page knob.';
  return '';
}
