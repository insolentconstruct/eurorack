#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

# Publish the GitHub release for a drizzlet-* tag from a build's artifacts.
# Usage: publish.sh TAG ARTIFACT_DIR   (needs GH_TOKEN and GITHUB_REPOSITORY)
set -euo pipefail
tag=${1:?usage: publish.sh TAG ARTIFACT_DIR}
dir=${2:?usage: publish.sh TAG ARTIFACT_DIR}
repo=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is not set}
[[ $tag =~ ^drizzlet-[A-Za-z0-9._-]+$ ]] || { echo "Bad tag: $tag" >&2; exit 2; }

title="Drizzlet ${tag#drizzlet-}"
prerelease=false
notes="Alternative firmware for Mutable Instruments Beads, built on the released Mutable Instruments source; not a Mutable Instruments product. Read the [README](https://github.com/$repo/blob/$tag/beads/build/README.md) first. How it was tested: [TESTING.md](https://github.com/$repo/blob/$tag/beads/build/docs/TESTING.md)."
# This release's CHANGELOG.md section, under its "## VERSION" heading.
changelog=${CHANGELOG:-beads/build/CHANGELOG.md}
changes=$(awk -v v="${tag#drizzlet-}" '/^## /{p = $2 == v; next} p' "$changelog")
[[ -n ${changes//[[:space:]]/} ]] || { echo "No CHANGELOG.md section for ${tag#drizzlet-}" >&2; exit 1; }
# A release can replace the Changes section with its own page,
# beads/build/release-notes/VERSION.md; {TAG} there becomes the tag.
page=${RELEASE_NOTES:-beads/build/release-notes/${tag#drizzlet-}.md}
[[ -z ${RELEASE_NOTES:-} || -f $page ]] || { echo "No release notes page: $page" >&2; exit 1; }
if [[ -f $page ]]; then
  notes+=$'\n\n'"$(sed "s/{TAG}/$tag/g" "$page")"
else
  notes+=$'\n\n## Changes\n'"$changes"
fi
# The heading's label, e.g. "alpha 2" from "## 2026.10.16 (tag `...`, alpha 2)", goes in the title.
heading=$(awk -v v="${tag#drizzlet-}" '/^## / && $2 == v {print; exit}' "$changelog")
if [[ $heading =~ ,\ ([^,()]+)\)$ ]]; then
  label=${BASH_REMATCH[1]}
  title+=" ($label)"
  # Alphas, release candidates and previews are pre-releases; betas are
  # public calls for testers and become the latest release.
  if [[ ${label,,} =~ (^|[^a-z])(alpha|rc|preview)([^a-z]|$) ]]; then prerelease=true; fi
fi
latest=true
[[ $prerelease == false ]] || latest=false
assets=("$dir/$tag-wav.zip" "$dir/$tag-combo.zip" "$dir/NOTICE.txt"
  "$dir/OPTIONS.md" "$dir/options-guide.svg" "$dir/options-guide-mobile.svg"
  "$dir/SHA256SUMS")

names() { for f in "$@"; do basename "$f"; done | sort; }

publish() {
  if gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
    # Re-run: make the existing release match this build.
    # Only a pre-release gives up Latest here; a re-run never moves Latest back.
    local keep=()
    [[ $latest == true ]] || keep=(--latest=false)
    gh release edit "$tag" --repo "$repo" --draft=false --prerelease=$prerelease \
      "${keep[@]}" --title "$title" --notes "$notes"
    gh release upload "$tag" --repo "$repo" --clobber "${assets[@]}"
    local wanted name
    wanted=$(names "${assets[@]}")
    gh release view "$tag" --repo "$repo" --json assets -q '.assets[].name' |
      while IFS= read -r name; do
        grep -qxF "$name" <<<"$wanted" || gh release delete-asset "$tag" "$name" --repo "$repo" --yes </dev/null
      done
  else
    gh release create "$tag" --repo "$repo" --verify-tag --prerelease=$prerelease \
      --latest=$latest \
      --title "$title" --notes "$notes" "${assets[@]}"
  fi
}

verify() {
  local out state
  out=$(mktemp -d)
  gh release download "$tag" --repo "$repo" --dir "$out"
  (cd "$out" && sha256sum -c SHA256SUMS)
  for f in "${assets[@]}"; do cmp "$f" "$out/$(basename "$f")"; done
  diff <(names "${assets[@]}") <(find "$out" -maxdepth 1 -type f -printf '%f\n' | sort)
  state=$(gh release view "$tag" --repo "$repo" --json isDraft,isPrerelease -q '"\(.isDraft) \(.isPrerelease)"')
  [[ $state == "false $prerelease" ]] || { echo "Release is a draft, or its pre-release flag is not $prerelease" >&2; exit 1; }
}

(cd "$dir" && sha256sum -c SHA256SUMS)
publish
verify
