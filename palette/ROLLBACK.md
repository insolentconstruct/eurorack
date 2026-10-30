# Hygrostat rollback

The release owner authorizes and merges a rollback; the page maintainer prepares
it and verifies the deployed page. Record the last good Pages deployment, its
commit, and the last good release tag before each deploy.

1. **Bad page deploy:** prepare a PR against `drizzlet` reverting the page merge
   (`git revert -m 1 <page-merge-commit>` for a merge commit, or `git revert
   <page-squash-commit>` for a squash). Review the diff so it restores only the
   page and any page workflow changes, without reverting firmware. Run the page
   suites. The owner merges it; `.github/workflows/pages.yml` rebuilds and
   deploys because `palette/` changed. Do not rely on rerunning an old workflow:
   the branch must also contain the restored page for subsequent deploys.
2. **Bad release entry:** in the rollback PR, restore `palette/static/manifest.json`
   and the corresponding `palette/static/images/<previous-tag>/app.bin` from the
   last good page commit. Restore the complete entry (profile catalogue, CPU
   data, source/tag and hashes), remove the bad entry from the offered releases,
   and run `node palette/tools/version-assets.js`. Do not just rename a tag or
   reuse its image URL. If the previous release is Beta 1 (plain WAV), restore
   the complete previous builder as in step 1: this profile editor requires a
   profile-capable release and cannot offer a Beta 1 manifest entry.
3. The owner marks a withdrawn GitHub release as a pre-release and explicitly
   makes the previous good published release Latest. Keep existing assets and
   tags for users who already downloaded them; document the withdrawal.
4. After the Pages deployment succeeds, reload the live page (allow up to ten
   minutes for cached HTML). Check the offered release, make its default WAV
   and compare its SHA-256 with the restored manifest, then check a custom
   profile and both guides if that release supports them. A page rollback does
   not downgrade a module; users reinstall the previous release's update WAV.
