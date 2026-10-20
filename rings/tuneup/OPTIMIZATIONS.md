# Rings optimizations

None retained yet. Candidates, each measured before it is kept:

- Carrying the string's interpolation and history state in locals.
- Loading all all-pass tails before any stores in the reverb.
- Integer Hermite reads, if the delay lines interpolate stored integers in float.
