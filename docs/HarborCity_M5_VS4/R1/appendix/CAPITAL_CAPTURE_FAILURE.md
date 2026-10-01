# Packaged Capital Capture Failure

- Original run `m5_vs3_r3_b_20261001_170328`: FAIL_MAP_MISMATCH. All 25 named capital screenshots actually show L_HarborTown. Native results also report L_HarborTown. These files and their zero-failure harness result are retained, not accepted as B1 evidence.
- Root cause verified in local UE 5.8.2 GameInstance.cpp: Shipping disables command-line startup map overrides. This is not an asset-cooking failure; the separate production-menu roundtrip reports L_CapitalPreview in its middle phase.
- Follow-up capture changes only the isolated test User/Saved/Config/Engine.ini GameDefaultMap. The candidate EXE, packaged config/content and unique full regression remain unchanged. Launcher now checks the actual results map.
- This is a separately disclosed capture-fixture correction, not a replacement or aggregation of the failed run. Corrected image results must be inspected before acceptance.
- Corrected isolated capture `m5_vs3_r3_b_20261001_171946` reports L_CapitalPreview. Contact-sheet inspection confirms the 500 x 450 m whitebox. It still has a visible rectangular terrain boundary and incomplete foreground/midground/landmark composition; B2 visual quality is not accepted.
