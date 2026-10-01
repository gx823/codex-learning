# Sprint Session 04 / Incomplete Terminal Evidence

`m5_vs3_r3_b_20261001_174214` exited with code 0 at 17:44:12. Its immutable results.json still says RUNNING, with all 32 expected checks and 0 assertion failures. All 14 case CSVs exist, including final FP_reload_01. Host samples remained AC=1/unlocked; no Escape latch or stop marker was observed. No game process remains.

The cause of the missing COMPLETE write is unverified. A normal-looking exit and expected check count do not authorize rewriting the native result as PASS. Source WriteReport ignores the save return value, so failed terminal output is one hypothesis, not an established cause.

The queue stopped automatically. Session 04 is retained without replacement; sessions 01-03 are not repeated. Only the never-started sessions 05-10 continue in the same explicitly authorized ten-session fixture. Their raw samples remain inspectable, but a complete ten-session acceptance PASS is withheld while any terminal report is incomplete. No game source or final EXE is changed.

Session 05 (`m5_vs3_r3_b_20261001_174742`) subsequently lost foreground focus. The wrapper closed its window and recorded BLOCKED_FOCUS; native EndPlay recorded `Test interrupted` FAIL after 5.55 seconds, before the sprint CSV samples. The native status COMPLETE reflects shutdown finalization, not completion of the intended test. No Escape stop was recorded. No game process remains. Sessions 06-10 have not been started; no automatic focus-stealing loop or replacement run.
