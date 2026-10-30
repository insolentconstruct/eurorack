// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Knob cards port options_guide.py: 270-degree arcs and the same LED ladder.
import { defaults, equal, encodeProfile } from './profile.js?v=9f8a0284597bc23e';
import { RELEASE_GUIDES, RELEASE_GUIDE_ID } from './release_guides.js?v=9f8a0284597bc23e';

const KNOBS = ['FEEDBACK', 'DRY/WET', 'REVERB'];
export const ACTIONS = ['None', '[A]+SEED', '[A]+FREEZE', '[A] hold'];
export const AMOUNTS = ['None', '[A]+DRY/WET', '[A]+REVERB'];
export const pageName = (slot) => slot ? `[M] page ${Math.ceil(slot / 3)} · ${KNOBS[(slot - 1) % 3]}` : 'None';
const escape = (s) => String(s).replace(/[&<>"']/g, (c) =>
  ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#x27;' })[c]);
const COLOURS = ['#247443', '#a87900', '#bd3030'];
export function indication(index) {
  const colour = index === 9 ? 'yellow' : ['green', 'yellow', 'red'][index % 3];
  return `${colour}; ${index === 9 ? 'two pulses' : index >= 6 ? 'fast blink' : index >= 3 ? 'slow blink' : 'steady'}`;
}
class Drawing {
  constructor() { this.items = []; }
  text(x, y, value, size = 15, weight = 'normal', colour = '#222', anchor = 'start') {
    this.items.push(`<text x="${x}" y="${y}" font-size="${size}" font-weight="${weight}" fill="${colour}" text-anchor="${anchor}">${escape(value)}</text>`);
  }
  lines(x, y, value, columns) {
    let line = '';
    for (const word of value.split(/\s+/)) {
      if (line && line.length + word.length + 1 > columns) {
        this.text(x, y, line); y += 21; line = '';
      }
      line += (line ? ' ' : '') + word;
    }
    if (line) { this.text(x, y, line); y += 21; }
    return y;
  }
  circle(x, y, r, colour = 'white', index = 0) {
    const dash = index >= 6 && index < 9 ? ' stroke-dasharray="1 3"' :
      index >= 3 && index < 6 ? ' stroke-dasharray="6 4"' : '';
    this.items.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${colour}" stroke="#333" stroke-width="2"${dash}/>`);
    if (index === 9) { this.circle(x - 4, y, 1, '#222'); this.circle(x + 4, y, 1, '#222'); }
  }
  arc(x, y, a, b, band = false) {
    const point = (t) => {
      const angle = (135 + 270 * t) * Math.PI / 180;
      return [x + 37 * Math.cos(angle), y + 37 * Math.sin(angle)];
    };
    const [x1, y1] = point(band ? a : a + .012);
    const [x2, y2] = point(band ? b : b - .012);
    this.items.push(`<path d="M ${x1.toFixed(3)} ${y1.toFixed(3)} A 37 37 0 ${Number(b - a > .6667)} 1 ${x2.toFixed(3)} ${y2.toFixed(3)}" fill="none" stroke="${band ? '#aaa' : '#555'}" stroke-width="${band ? 8 : 3}"/>`);
  }
  card(x, y, width, f, entry, title, mobile) {
    this.text(x, y, title, 17, 'bold');
    const cx = x + width / 2, cy = y + 72;
    this.circle(cx, cy, 22);
    this.items.push(`<path d="M ${cx} ${cy} v -16" stroke="#222" stroke-width="2"/>`);
    const count = f.id === 1 ? 3 : f.values ? f.values.length : 2;
    const bands = f.id === 12 ? [] : f.id === 11 ? [[.07, .13]] : count === 2 ? [[.47, .53]] : Array.from({ length: count - 1 }, (_, i) => [(i + 1) / count - .03, (i + 1) / count + .03]);
    const starts = [0, ...bands.map((b) => b[1])];
    const ends = [...bands.map((b) => b[0]), 1];
    const zoneLabels = f.id === 12 ? ['more →'] : f.id === 1 ? [f.values[0], f.values[1], 'Scale'] :
      f.values || (f.id === 11 ? ['Stock', 'wider →'] : ['None', 'more →']);
    starts.forEach((a, i) => {
      this.arc(cx, cy, a, ends[i]);
      const angle = (135 + 270 * (a + ends[i]) / 2) * Math.PI / 180;
      const lx = cx + 53 * Math.cos(angle), ly = cy + 53 * Math.sin(angle);
      const anchor = lx < cx - 15 ? 'end' : lx > cx + 15 ? 'start' : 'middle';
      this.text(Number(lx.toFixed(2)), Number((ly + 5).toFixed(2)),
          zoneLabels[i], 15, 'normal', '#222', anchor);
    });
    bands.forEach(([a, b]) => this.arc(cx, cy, a, b, true));
    let base = y + 125;
    const values = f.values || (f.id === 11 ? ['Stock', 'Width (wider →)'] : ['None', 'Diffusion (more →)']);
    values.forEach((value, i) => {
      if (f.id === 1 && i === 2) {
        this.text(x, base, 'Scale (tap SEED to step through):', 15, 'bold'); base += 26;
      }
      const meter = !f.values && i === 1;
      const shown = meter ? 'green → yellow → red' : indication(i);
      this.circle(x + 9, base - 5, 8, meter ? '#777' : COLOURS[i === 9 ? 1 : i % 3], i);
      this.text(x + 25, base, value);
      if (mobile) {
        this.text(x + 25, base + 19, shown, 15, 'normal', '#555');
      } else {
        this.text(x + width - 3, base, shown.replace(' blink', ''), 15, 'normal', '#555', 'end');
      }
      base += mobile ? 44 : 21;
    });
    this.text(x, base, `Default: ${f.values ? f.values[entry[2]] : entry[2]}`, 15, 'bold');
    if (f.id === 4) {
      base += 26;
      this.text(x, base, 'Switching clears the recording.', 15, 'bold', COLOURS[2]);
    }
    return base + 30 - y;
  }
}

export function guide(entries, release, fp, mobile = false) {
  if (release.profile.release_id === RELEASE_GUIDE_ID && equal(encodeProfile(entries, release.profile),
      encodeProfile(defaults(release.profile), release.profile))) {
    // Normalisation removes this added fingerprint text; the remaining SVG
    // is byte-identical to options_guide.py's release drawing.
    return RELEASE_GUIDES[Number(mobile)].replace('</g>\n</svg>',
        `<text data-profile-fingerprint="true" x="${mobile ? 22 : 35}" y="52" font-size="12">Profile ${escape(fp)}</text>\n</g>\n</svg>`);
  }
  const d = new Drawing();
  const width = mobile ? 390 : 1100;
  const margin = mobile ? 22 : 35;
  const features = release.profile.features;
  const feature = (e) => features.find((f) => f.id === e[0]);
  d.text(margin, 35, 'Drizzlet Hidden Options', 24, 'bold');
  d.text(margin, 61, `Profile ${fp} · ${release.tag}`, 15);
  let y = 90;
  for (const line of (entries.some((e) => e[3]) ? [
    'Open: hold the CV-assign button [M] 3 seconds, release.',
    'Change page: tap [M]. Empty pages are skipped. Pages 1–4 pulse FEEDBACK, DRY/WET, REVERB, then all three.',
    '[A] shows each stored value in turn. Turn a knob to change it; it saves by itself.',
    'Close: hold [M] 3 seconds or press another button. With Scale selected, SEED taps step the scale.',
    'Move a knob a little before it responds. Wait a moment before powering off.'
  ] : ['This profile has no options pages. Hold [M] does nothing.'])) y = d.lines(margin, y, line, mobile ? 43 : 140) + 3;
  // Share the release's to-scale control sketch and LED legend.
  const reference = RELEASE_GUIDES[0];
  const panelStart = reference.indexOf('<text x="405" y="70"');
  const panelEnd = reference.indexOf('<text x="780" y="70"');
  if (panelStart >= 0 && panelEnd > panelStart) {
    d.items.push(`<g transform="translate(${margin} ${y}) scale(${mobile ? 0.85 : 1}) translate(-405 -70)">${reference.slice(panelStart, panelEnd)}</g>`);
    y += 285;
  }
  for (let page = 1; page <= 4; ++page) {
    const placed = entries.filter((e) => e[3] && Math.ceil(e[3] / 3) === page);
    if (!placed.length) continue;
    y += 20;
    d.text(margin, y, `PAGE ${page} — ${page === 4 ? 'all three LEDs' : KNOBS[page - 1] + ' LED'} pulsing`, 20, 'bold');
    y += 34;
    if (mobile) {
      for (const e of placed) y += d.card(margin, y, width - margin * 2,
          feature(e), e, `${KNOBS[(e[3] - 1) % 3]}: ${feature(e).name}`, true) + 20;
    } else {
      const heights = placed.map((e) => d.card(margin + ((e[3] - 1) % 3) * 345,
          y, 325, feature(e), e, `${KNOBS[(e[3] - 1) % 3]}: ${feature(e).name}`, false));
      y += Math.max(...heights) + 20;
    }
  }
  y += 20;
  d.text(margin, y, 'OFF THE PAGES', 20, 'bold'); y += 34;
  for (const e of entries.filter((e) => e[4])) {
    y += d.card(margin, y, mobile ? width - margin * 2 : 325,
        feature(e), e, `${AMOUNTS[e[4]]}: ${feature(e).name}`, mobile) + 15;
  }
  y = d.lines(margin, y, 'Manual gain: hold [A] and turn FEEDBACK. [A] hold resets gain when no hold action runs.', mobile ? 43 : 140);
  for (const e of entries.filter((e) => e[5])) {
    const extra = e[0] === 9 ? ' With Synth start on Manual, both inputs unplugged and FREEZE off.' :
        e[0] === 13 ? ' Toggle monophonic grains.' : ' Start a fresh recording window.';
    y = d.lines(margin, y + 10, `${ACTIONS[e[5]]}: ${feature(e).name}.${e[5] === 3 ? ' Both inputs unplugged.' : ''}${extra}`, mobile ? 43 : 140);
  }
  for (const e of entries.filter((e) => e[1] === 1 && !e[5])) {
    const f = feature(e);
    y = d.lines(margin, y + 10, `${f.name}: Fixed · ${f.values ? f.values[e[2]] : e[2]}`, mobile ? 43 : 140);
  }
  for (const e of entries.filter((e) => e[1] === 2 && !e[3] && !e[4] && !e[5])) {
    const f = feature(e);
    y = d.lines(margin, y + 10, `${f.name}: ${f.values ? f.values[e[2]] : e[2]} · set on the page: not placed`, mobile ? 43 : 140);
  }
  y = d.lines(margin, y + 20, 'Do not hold SEED while powering on: it resets all settings to the profile defaults. Calibration is kept.', mobile ? 43 : 140);
  d.text(margin, y + 15, `Author: Insolent Construct · Profile ${fp}`, 15);
  y = d.lines(margin, y + 40, 'Panel layout from the Mutable Instruments Beads panel file, geometry only (CC BY-SA 3.0).', mobile ? 43 : 140);
  const height = y + 30;
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${mobile ? 390 : '190mm'}" height="${mobile ? height : (190 * height / width).toFixed(2) + 'mm'}" viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="title"><title id="title">Drizzlet hidden options guide · ${escape(fp)}</title><metadata>Copyright 2026 Insolent Construct. Author: Insolent Construct (insolentconstruct@gmail.com). Panel geometry adapted from Mutable Instruments Beads, CC BY-SA 3.0 (https://creativecommons.org/licenses/by-sa/3.0/).</metadata><rect width="100%" height="100%" fill="white"/><g font-family="sans-serif" fill="#222">\n${d.items.join('\n')}\n</g>\n</svg>\n`;
}
